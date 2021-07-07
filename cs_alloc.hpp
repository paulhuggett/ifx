//
//  cs_alloc.hpp
//  ifx
//
//  Created by Paul Bowen-Huggett on 30/06/2021.
//
#ifndef CS_ALLOC_HPP
#define CS_ALLOC_HPP

#include "pstore/adt/chunked_sequence.hpp"

/// Increases the number of elements in a byte-wide chunked sequence by \p required ensuring that
/// the resulting storage is contiguous.
///
/// \param storage  A chunked-sequence which will be used to manage the storage.
/// \param required  The number of contiguous bytes required.
/// \param align  The required alignment for the start of the returned storage.
/// \result A pointer to a contiguous block of storage which is sufficient for \p required bytes.
template <std::size_t ElementsPerChunk>
void * cs_alloc (pstore::chunked_sequence<std::uint8_t, ElementsPerChunk> * const storage,
                 std::size_t required, std::size_t const align) {
    assert (storage != nullptr && "Storage must not be null");
    assert (required <= ElementsPerChunk);
    if (required == 0U) {
        return nullptr;
    }
    std::size_t const capacity = storage->capacity ();
    std::size_t size = storage->size ();
    assert (capacity >= size && "Capacity cannot be less than size");
    if (capacity - (size + align - 1U) < required) {
        // A resize to burn through the remaining members of the container's final
        // chunk.
        storage->resize (capacity);
        size = capacity;
    }
    // Add a default-constructed value. This is the first element of the returned array and gets us
    // the starting address.
    auto * result = &storage->emplace_back ();
    ++size;
    auto misaligned = reinterpret_cast<uintptr_t> (result) % align;
    if (misaligned != 0) {
        required += align - misaligned;
        result += align - misaligned;
    }
    assert (storage->size () == size && "Size didn't track the container size correctly");
    storage->resize (size + required - 1U);
    return result;
}

#endif // CS_ALLOC_HPP
