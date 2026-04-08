namespace jreng
{
/*____________________________________________________________________________*/
/**
 * @brief A responsible global context wrapper.
 *
 * Provides safe, explicit, and deterministic global access to a single live
 * instance of a given type without enforcing ownership. This allows global
 * reachability without turning the object into a true singleton.
 *
 * Design intent:
 * - Only one active instance of a given type is allowed at any time.
 * - Ownership remains explicit; lifetime is managed externally.
 * - Access is fail-fast and self-checking via jassert.
 * - Compile-time safety: ensures the derived class passes itself as the template parameter.
 *
 * Usage example:
 * @code
 * struct Registry : Context<Registry>  // ✓ Correct: passes itself
 * {
 *     // your members...
 * };
 *
 * struct MyClass : Context<Registry>     // ✗ Error: should be Context<MyClass>
 * {
 *     // Compile error: Context<T> must be used with the derived class itself
 * };
 *
 * void setup()
 * {
 *     Registry registry;               // sets global context
 *     auto& ctx = Registry::getContext(); // safely access it
 * }
 * @endcode
 *
 * @tparam ObjectType The class type deriving from Context. MUST be the derived class itself.
 */
template<typename ObjectType>
struct Context
{
public:
    /**
     * @brief Constructs the context and registers it as the active instance.
     *
     * Ensures only one active instance of the given ObjectType exists.
     * Asserts if another instance is already registered.
     *
     * Compile-time check: verifies that the derived class correctly passes itself
     * as the template parameter to prevent accidental context collisions.
     */
    Context()
    {
        // Compile-time safety: ensure the derived class passes itself as ObjectType
        // This prevents accidental misuse like: class WrongClass : public Context<SomeOtherType> { }
        // The static_assert checks that ObjectType actually derives from Context<ObjectType>,
        // which is only true if used correctly in the CRTP pattern.
        static_assert (std::is_base_of<Context<ObjectType>, ObjectType>::value,
                       "Context<T> must be used with the derived class itself. "
                       "Correct usage: class MyClass : public Context<MyClass> { }");

        jassert (context == nullptr
                 && "Context already exists! Only one instance of this type is allowed at a time.");
        context = static_cast<ObjectType*> (this);
    }

    /**
     * @brief Destructor that unregisters the active context.
     *
     * Asserts if the context being destroyed is not the one currently active.
     */
    virtual ~Context()
    {
        jassert (context == this
                 && "Destroying a different instance than the active context!");
        context = nullptr;
    }

    /**
     * @brief Returns a reference to the current active context.
     *
     * Asserts if no active instance exists. Use this only when you are sure
     * the context has been properly constructed.
     *
     * @return Reference to the active ObjectType instance.
     */
    static ObjectType* getContext() noexcept {
        jassert (context != nullptr
                 && "Context is not set! Ensure an instance exists before calling getContext().");
        return context;
    }

private:
    inline static ObjectType* context = nullptr; ///< Pointer to the active context instance.

    Context (const Context&) = delete;            ///< Non-copyable.
    Context& operator= (const Context&) = delete; ///< Non-assignable.
    Context (Context&&) = delete;                 ///< Non-movable.
    Context& operator= (Context&&) = delete;      ///< Non-move assignable.
};


/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
