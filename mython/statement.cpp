#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace {
        const string ADD_METHOD = "__add__"s;
        const string INIT_METHOD = "__init__"s;
    }  // namespace

    ObjectHolder Assignment::Execute(Closure& closure, Context& context)
    {
        closure[var_] = rv_->Execute(closure, context);

        return closure[var_];
    }

    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv) : var_(var), rv_(std::move(rv))
    {
    }

    VariableValue::VariableValue(const std::string& var_name) : var_name_(var_name)
    {
    }

    VariableValue::VariableValue(std::vector<std::string> dotted_ids) : var_name_(dotted_ids[0])
    {
        for(size_t i = 1; i < dotted_ids.size(); i++)
        {
            dotted_ids_.push_back(std::move(dotted_ids[i]));
        }
    }

    ObjectHolder VariableValue::Execute(Closure& closure, Context&)
    {
        auto it = closure.find(var_name_);
        if(it != closure.end())
        {
            ObjectHolder object_holder = it->second;
            for(const auto& dotted_id : dotted_ids_)
            {
                if(auto p = object_holder.TryAs<runtime::ClassInstance>())
                {
                    Closure &fields = p->Fields();
                    object_holder = fields.at(dotted_id);
                }
            }
            return object_holder;
        }
        else
        {
            throw std::runtime_error("Variable \""s + var_name_ + "\" is not found"s);
        }
    }

    unique_ptr<Print> Print::Variable(const std::string& name)
    {
        return std::make_unique<Print>(std::make_unique<VariableValue>(name));
    }

    Print::Print(unique_ptr<Statement> argument)
    {
        args_.push_back(std::move(argument));
    }

    Print::Print(vector<unique_ptr<Statement>> args) : args_(std::move(args))
    {
    }

    ObjectHolder Print::Execute(Closure& closure, Context& context)
    {
        ostream& out = context.GetOutputStream();

        size_t args_size = args_.size();
        for(size_t i = 0; i < args_size; ++i)
        {
            if(ObjectHolder holder = args_[i]->Execute(closure, context))
            {
                holder->Print(out, context);
            }
            else
            {
                out << "None";
            }

            if(i != args_size-1)
            {
                out << ' ';
            }
        }
        out << '\n';

        return ObjectHolder::None();
    }

    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method, std::vector<std::unique_ptr<Statement>> args)
        : object_(std::move(object)), method_(method), args_(std::move(args))
    {
    }

    ObjectHolder MethodCall::Execute(Closure& closure, Context& context)
    {
        if(const auto class_instance = object_->Execute(closure, context).TryAs<runtime::ClassInstance>())
        {
            std::vector<runtime::ObjectHolder> cur_args;
            for(const auto &arg : args_)
            {
                cur_args.push_back(arg->Execute(closure, context));
            }
            return class_instance->Call(method_, cur_args, context);
        }

        return ObjectHolder::None();
    }

    ObjectHolder Stringify::Execute(Closure& closure, Context& context)
    {
        if(ObjectHolder holder = argument_->Execute(closure, context))
        {
            std::ostringstream out;
            holder->Print(out, context);
            return ObjectHolder::Own(runtime::String(out.str()));
        }

        return ObjectHolder::Own(runtime::String("None"));
    }

    ObjectHolder Add::Execute(Closure& closure, Context& context)
    {
        auto lhs_holder = lhs_->Execute(closure, context);
        auto rhs_holder = rhs_->Execute(closure, context);

        if(auto lhs_holder_number = lhs_holder.TryAs<runtime::Number>())
        {
            if(auto rhs_holder_number = rhs_holder.TryAs<runtime::Number>())
            {
                return ObjectHolder::Own(runtime::Number(lhs_holder_number->GetValue() + rhs_holder_number->GetValue()));
            }
        }

        if(auto lhs_holder_number = lhs_holder.TryAs<runtime::String>())
        {
            if(auto rhs_holder_number = rhs_holder.TryAs<runtime::String>())
            {
                return ObjectHolder::Own(runtime::String(lhs_holder_number->GetValue() + rhs_holder_number->GetValue()));
            }
        }

        if(auto lhs_class_instance = lhs_->Execute(closure, context).TryAs<runtime::ClassInstance>())
        {
            auto holder = rhs_->Execute(closure, context);
            return lhs_class_instance->Call(ADD_METHOD, {holder}, context);
        }

        throw std::runtime_error("Object addition operation error"s);
    }

    ObjectHolder Sub::Execute(Closure& closure, Context& context)
    {
        auto lhs_obj = lhs_->Execute(closure, context).TryAs<runtime::Number>();
        auto rhs_obj = rhs_->Execute(closure, context).TryAs<runtime::Number>();

        if(lhs_obj != nullptr && rhs_obj != nullptr)
        {
            return ObjectHolder::Own(runtime::Number(lhs_obj->GetValue() - rhs_obj->GetValue()));
        }

        throw std::runtime_error("Object subtraction operation error"s);
    }

    ObjectHolder Mult::Execute(Closure& closure, Context& context)
    {
        auto lhs_obj = lhs_->Execute(closure, context).TryAs<runtime::Number>();
        auto rhs_obj = rhs_->Execute(closure, context).TryAs<runtime::Number>();

        if(lhs_obj != nullptr && rhs_obj != nullptr)
        {
            return ObjectHolder::Own(runtime::Number(lhs_obj->GetValue() * rhs_obj->GetValue()));
        }

        throw std::runtime_error("Object multiplication operation error"s);
    }

    ObjectHolder Div::Execute(Closure& closure, Context& context)
    {
        auto lhs_obj = lhs_->Execute(closure, context).TryAs<runtime::Number>();
        auto rhs_obj = rhs_->Execute(closure, context).TryAs<runtime::Number>();

        if(lhs_obj != nullptr && rhs_obj != nullptr)
        {
            return ObjectHolder::Own(runtime::Number(lhs_obj->GetValue() / rhs_obj->GetValue()));
        }

        throw std::runtime_error("Object division operation error"s);
    }

    ObjectHolder Compound::Execute(Closure& closure, Context& context)
    {
        for(auto& argument : args_)
        {
            argument->Execute(closure, context);
        }

        return ObjectHolder::None();
    }

    ObjectHolder Return::Execute(Closure& closure, Context& context)
    {
        throw statement_->Execute(closure, context);
    }

    ClassDefinition::ClassDefinition(ObjectHolder cls) : cls_(cls)
    {
    }

    ObjectHolder ClassDefinition::Execute(Closure& closure, Context&)
    {
        if(runtime::Class* cls = cls_.TryAs<runtime::Class>())
        {
            closure[cls->GetName()] = cls_;
            return cls_;
        }

        return ObjectHolder::None();
    }

    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name, std::unique_ptr<Statement> rv)
        : object_(object), field_name_(field_name), rv_(std::move(rv))
    {
    }

    ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context)
    {
        auto class_instance = object_.Execute(closure, context).TryAs<runtime::ClassInstance>();
        if(class_instance != nullptr)
        {
            return class_instance->Fields()[field_name_] = rv_->Execute(closure, context);
        }

        return ObjectHolder::None();
    }

    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body, std::unique_ptr<Statement> else_body)
        : condition_(std::move(condition)), if_body_(std::move(if_body)), else_body_(std::move(else_body))
    {
    }

    ObjectHolder IfElse::Execute(Closure& closure, Context& context)
    {
        if(runtime::IsTrue(condition_->Execute(closure, context)))
        {
            return if_body_->Execute(closure, context);
        }
        else if(else_body_ != nullptr)
        {
            return else_body_->Execute(closure, context);
        }

        return ObjectHolder::None();
    }

    ObjectHolder Or::Execute(Closure& closure, Context& context)
    {
        if(runtime::IsTrue(lhs_->Execute(closure, context)) || runtime::IsTrue(rhs_->Execute(closure, context)))
        {
            return ObjectHolder::Own(runtime::Bool(true));
        }

        return ObjectHolder::Own(runtime::Bool(false));
    }

    ObjectHolder And::Execute(Closure& closure, Context& context)
    {
        if(runtime::IsTrue(lhs_->Execute(closure, context)) && runtime::IsTrue(rhs_->Execute(closure, context)))
        {
            return ObjectHolder::Own(runtime::Bool(true));
        }

        return ObjectHolder::Own(runtime::Bool(false));
    }

    ObjectHolder Not::Execute(Closure& closure, Context& context)
    {
        auto result = !(runtime::IsTrue(argument_->Execute(closure, context)));

        return ObjectHolder::Own(runtime::Bool(result));
    }

    Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
            : BinaryOperation(std::move(lhs), std::move(rhs)), cmp_(cmp)
    {
    }

    ObjectHolder Comparison::Execute(Closure& closure, Context& context)
    {
        auto lhs_holder = lhs_->Execute(closure, context);
        auto rhs_holder = rhs_->Execute(closure, context);

        bool comparison_result = cmp_(lhs_holder, rhs_holder, context);
        return ObjectHolder::Own(runtime::Bool(comparison_result));
    }

    NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args)
        : class_instance_(class_), args_(std::move(args))
    {
    }

    NewInstance::NewInstance(const runtime::Class& cls) : class_instance_(cls)
    {
    }

    ObjectHolder NewInstance::Execute(Closure& closure, Context& context)
    {
        if(class_instance_.HasMethod(INIT_METHOD, args_.size()))
        {
            vector<ObjectHolder> result_args;
            for(const auto& arg : args_)
            {
                result_args.push_back(arg.get()->Execute(closure, context));
            }
            class_instance_.Call(INIT_METHOD, result_args, context);
        }

        return ObjectHolder::Share(class_instance_);
    }

    MethodBody::MethodBody(std::unique_ptr<Statement>&& body) : body_(std::move(body))
    {
    }

    ObjectHolder MethodBody::Execute(Closure& closure, Context& context)
    {
        try
        {
            body_->Execute(closure, context);
            return runtime::ObjectHolder::None();
        }
        catch (ObjectHolder& obj)
        {
            return obj;
        }
    }

}  // namespace ast
