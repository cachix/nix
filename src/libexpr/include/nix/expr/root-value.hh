#pragma once
///@file

#include <memory>
#include <utility>

namespace nix {

struct Value;

/**
 * A move-only handle keeping a GC object and everything reachable from it
 * alive. Root slots are recycled through a pool shared with RootValue.
 */
class RootObject
{
    const void ** slot = nullptr;

    /**
     * Clear the given slot and return it to the root value pool.
     */
    void freeRootObjectSlot();

public:
    RootObject() = default;

    /**
     * Allocate a slot from the root value pool, i.e. a GC-visible
     * pointer cell that keeps the value it points to alive across
     * garbage collections.
     */
    explicit RootObject(const void * v);

    RootObject(const RootObject &) = delete;
    RootObject & operator=(const RootObject &) = delete;

    RootObject(RootObject && other) noexcept
        : slot(std::exchange(other.slot, nullptr))
    {
    }

    RootObject & operator=(RootObject && other) noexcept
    {
        if (slot)
            freeRootObjectSlot();
        slot = std::exchange(other.slot, nullptr);
        return *this;
    }

    ~RootObject()
    {
        reset();
    }

    /**
     * Release the slot, i.e. stop rooting the value.
     */
    void reset()
    {
        if (slot)
            freeRootObjectSlot();
    }

    /** Update the object held by an allocated root slot. */
    void set(const void * p)
    {
        *slot = p;
    }

    const void * operator*() const
    {
        return *slot;
    }

    explicit operator bool() const
    {
        return slot != nullptr;
    }
};

/** A typed root handle for evaluator values. */
class RootValue : public RootObject
{
public:
    RootValue() = default;

    explicit RootValue(Value * v)
        : RootObject(v)
    {
    }

    void set(Value * v)
    {
        RootObject::set(v);
    }

    Value * operator*() const
    {
        return static_cast<Value *>(const_cast<void *>(RootObject::operator*()));
    }
};

} // namespace nix
