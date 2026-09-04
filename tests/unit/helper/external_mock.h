#ifndef __TESTS_UNIT_HELPER_EXTERNAL_MOCK__
#define __TESTS_UNIT_HELPER_EXTERNAL_MOCK__

#include <functional>

template <typename T>
class ExternalMock : public T
{
public:
    template <typename... Args>
    ExternalMock(Args&&... args) : T(std::forward<Args>(args)...)
    {
        for (auto &expectation : ExternalMock<T>::operations)
        {
            if (expectation)
            {
                expectation(this);
            }
        }

        ExternalMock<T>::instantiated = true;
        ExternalMock<T>::operations.clear();

        T::ctor(std::forward<Args>(args)...);
    }

    static void Initialize()
    {
        ExternalMock<T>::instantiated = false;
        ExternalMock<T>::operations.clear();
    }

    static void AddOperation(std::function<void(T*)> operation)
    {
        ExternalMock<T>::operations.push_back(operation);
    }

    static std::size_t TotalOperations()
    {
        return ExternalMock<T>::operations.size();
    }

    static bool HasBeenInstantiated()
    {
        return ExternalMock<T>::instantiated;
    }

private:
    inline static bool instantiated{false};
    inline static std::vector<std::function<void(T*)>> operations{};
};

#endif
