#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
            : data_(std::move(data)) {
    }

    void ObjectHolder::AssertIsValid() const {
        assert(data_ != nullptr);
    }

    ObjectHolder ObjectHolder::Share(Object& object) {
        // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
        return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
    }

    ObjectHolder ObjectHolder::None() {
        return ObjectHolder();
    }

    Object& ObjectHolder::operator*() const {
        AssertIsValid();
        return *Get();
    }

    Object* ObjectHolder::operator->() const {
        AssertIsValid();
        return Get();
    }

    Object* ObjectHolder::Get() const {
        return data_.get();
    }

    ObjectHolder::operator bool() const {
        return Get() != nullptr;
    }

    // Содержится ли в object значение, приводимое к True
    bool IsTrue(const ObjectHolder& object)
    {
        if(!object)
        {
            return false;
        }

        if(auto obj_num = object.TryAs<Number>())
        {
            return obj_num->GetValue();
        }

        if(auto obj_str = object.TryAs<String>())
        {
            return !(obj_str->GetValue().empty());
        }

        if(auto obj_bool = object.TryAs<Bool>())
        {
            return obj_bool->GetValue();
        }

        return false;
    }

    void ClassInstance::Print(std::ostream& os, Context& context)
    {
        if(HasMethod("__str__", 0))
        {
            Call("__str__", {}, context)->Print(os, context);
        }
        else
        {
            os << this;
        }
    }

    // Определяет, есть ли у объекта метод с указанным именем и количеством аргументов.
    bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const
    {
        auto m = cls_.GetMethod(method);
        if(m == nullptr || m->formal_params.size() != argument_count)
        {
            return false;
        }

        return true;
    }

    Closure& ClassInstance::Fields()
    {
        return fields_;
    }

    const Closure& ClassInstance::Fields() const
    {
        return fields_;
    }

    ClassInstance::ClassInstance(const Class& cls) : cls_(cls)
    {
    }

    // Вызывает метод класса, передавая ему аргументы.
    // Возвращает результат, возвращённый из метода.
    // Если ни класс, ни его родители не содержат метод с таким именем, выбрасывается исключение runtime_error.
    ObjectHolder ClassInstance::Call(const std::string& method,
                                     const std::vector<ObjectHolder>& actual_args,
                                     Context& context)
    {
        if(!HasMethod(method, actual_args.size()))
        {
            throw std::runtime_error("Method not found"s);
        }

        auto m = cls_.GetMethod(method);
        Closure closure;
        closure["self"s] = ObjectHolder::Share(*this);

        for(size_t i = 0; i < actual_args.size(); ++i)
        {
            closure[m->formal_params[i]] = actual_args[i];
        }

        return m->body->Execute(closure, context);
    }

    Class::Class(std::string name, std::vector<Method> methods, const Class* parent) : name_(name), parent_(parent)
    {
        for(auto& method : methods)
        {
            methods_[method.name] = std::move(method);
        }
    }

    const Method* Class::GetMethod(const std::string& name) const
    {
        // Поиск метода в классе
        if(methods_.count(name))
        {
            return &methods_.at(name);
        }

        // Поиск метода в родительских классах
        if(parent_ != nullptr)
        {
            return parent_->GetMethod(name);
        }

        return nullptr;
    }

    [[nodiscard]] const std::string& Class::GetName() const
    {
        return name_;
    }

    void Class::Print(ostream& os, Context&)
    {
        os << "Class " << name_;
    }

    void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
        os << (GetValue() ? "True"sv : "False"sv);
    }

    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context)
    {
        if(!lhs && !rhs)
        {
            return true;
        }

        if(auto lhs_obj = lhs.TryAs<Number>())
        {
            if(auto rhs_obj = rhs.TryAs<Number>())
            {
                return lhs_obj->GetValue() == rhs_obj->GetValue();
            }
        }

        if(auto lhs_obj = lhs.TryAs<String>())
        {
            if(auto rhs_obj = rhs.TryAs<String>())
            {
                return lhs_obj->GetValue() == rhs_obj->GetValue();
            }
        }

        if(auto lhs_obj = lhs.TryAs<Bool>())
        {
            if(auto rhs_obj = rhs.TryAs<Bool>())
            {
                return lhs_obj->GetValue() == rhs_obj->GetValue();
            }
        }

        if(auto lhs_class = lhs.TryAs<ClassInstance>())
        {
            if(lhs_class->HasMethod("__eq__"s, 1))
            {
                return IsTrue(lhs_class->Call("__eq__"s, {rhs}, context));
            }
        }

        throw std::runtime_error("Error compare (==) objects"s);
    }

    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context)
    {
        if(auto lhs_obj = lhs.TryAs<Number>())
        {
            if(auto rhs_obj = rhs.TryAs<Number>())
            {
                return lhs_obj->GetValue() < rhs_obj->GetValue();
            }
        }

        if(auto lhs_obj = lhs.TryAs<String>())
        {
            if(auto rhs_obj = rhs.TryAs<String>())
            {
                return lhs_obj->GetValue() < rhs_obj->GetValue();
            }
        }

        if(auto lhs_obj = lhs.TryAs<Bool>())
        {
            if(auto rhs_obj = rhs.TryAs<Bool>())
            {
                return lhs_obj->GetValue() < rhs_obj->GetValue();
            }
        }

        if(auto lhs_class = lhs.TryAs<ClassInstance>())
        {
            if (lhs_class->HasMethod("__lt__"s, 1))
            {
                return IsTrue(lhs_class->Call("__lt__"s, {rhs}, context));
            }
        }

        throw std::runtime_error("Error compare (<) objects"s);
    }

    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context)
    {
        return !Equal(lhs, rhs, context);
    }

    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context)
    {
        return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
    }

    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context)
    {
        return !Greater(lhs, rhs, context);
    }

    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context)
    {
        return !Less(lhs, rhs, context);
    }

}  // namespace runtime
