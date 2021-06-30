// Standard library
#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

// pstore
#include "pstore/adt/chunked_sequence.hpp"
#include "pstore/adt/sparse_array.hpp"
#include "pstore/mcrepo/section.hpp"

namespace {

    /// Increases the number of elements in a chunked sequence by \p required ensuring that the
    /// resulting storage is contiguous.
    ///
    /// \param storage  A chunked-sequence which will be used to manage the storage.
    /// \param required  The number of contiguous elements required.
    /// \result A pointer to a contiguous block of storage which is sufficient for \p required members.
    template <typename ChunkedSequence>
    typename ChunkedSequence::value_type *
    csalloc (ChunkedSequence * const storage, size_t required, size_t align) {
        assert (storage != nullptr);
        assert (required <= ChunkedSequence::elements_per_chunk);
        if (required == 0U) {
            return nullptr;
        }
        size_t const capacity = storage->capacity ();
        size_t size = storage->size ();
        assert (capacity >= size && "Capacity cannot be less than size");
        if (capacity - (size + align - 1U) < required) {
            // A resize to burn through the remaining members of the container's final
            // chunk.
            storage->resize (capacity);
            size = capacity;
        }
        // Add a default-constructed value. This is the first element of the returned array and gets us the starting address.
        auto * result = &storage->emplace_back ();
        ++size;
        auto misaligned = reinterpret_cast <uintptr_t> (result) % align;
        if (misaligned != 0) {
            required += align - misaligned;
            result += align - misaligned;
        }
        assert (storage->size () == size && "Size didn't track the container size correctly");
        storage->resize (size + required - 1U);
        return result;
    }



    using namespace pstore;
    using repo::section_kind;

    struct contribution;
    using contribution_array = sparse_array<contribution *, std::underlying_type_t<section_kind>>;

    struct contribution {
        constexpr contribution (section_kind skind, std::string const * const name,
                                contribution_array const * sections) noexcept
                : skind{skind}
                , name{name}
                , sections{sections} {}
        section_kind const skind;
        std::string const * const name;
        contribution_array const * const sections;
    };

    // Our super-minimal simulated fragment data structure.
    struct fragment {
        explicit fragment (std::string name, std::initializer_list<section_kind> sections) noexcept
                : name{std::move (name)}
                , sections{sections} {}
        // Fragments don't really have names. Here I use the name to show that the code is finding the correct fragment.
        std::string const name;
        // In the real implementation, this is a sparse array where the indices are the section numbers and the values are the offset to the start of the payload data associated with that section.
        std::set<section_kind> const sections;
    };

    using output_sections = std::map<section_kind, chunked_sequence<contribution>>;
    using fragment_to_contribution_map = std::map<fragment const *, contribution_array *>;

    // Simulating scan.
    //
    // Here we go through a compilation's definitions. Each name is entered into the symbol
    // table and, if it is being kept, we resolve all of the external fixups using the symbol
    // table. The final step is to allocate storage for a pointer to each output section
    // contribution: there is potentially one of these records per section in the fragment.
    fragment_to_contribution_map
    scan (chunked_sequence<uint8_t> * const storage, std::vector<fragment> const & fragments) {
        fragment_to_contribution_map captr;

        for (fragment const & f : fragments) {
            std::size_t const num_sections = f.sections.size ();
            if (num_sections < 2U) {
                // A fragment with only a single section cannot, by definition, have any internal
                // fixups to process since their purpose is to allow one section of a fragment to
                // reference another. That means that we don't need to allocate any storage for this
                // fragment.
                captr[&f] = nullptr;
            } else {
                auto * const ptr = csalloc (storage, contribution_array::size_bytes (num_sections), alignof (contribution_array));
                assert (reinterpret_cast<uintptr_t> (ptr) % alignof (contribution_array) == 0 &&
                        "Storage must be aligned correctly");
                captr[&f] =
                    new (ptr) contribution_array (std::begin (f.sections), std::end (f.sections));
            }
        }

        return captr;
    }

    // Simulating layout
    //
    // Layout assigns each section of each fragment to a specific output section. This is
    // recorded as a "contribution" which holds the associated address in target memory.
    output_sections
    layout (fragment_to_contribution_map const & captr) {
        output_sections outputs;

        for (auto const & c: captr) {
            auto * const f = std::get<fragment const * const> (c);
            auto * const ca = std::get<contribution_array *> (c);

            for (section_kind const section : f->sections) {
                auto & v = outputs[section];
                v.emplace_back (section, &f->name, ca); // build a contribution entry
                if (ca != nullptr) {
                    // Now that we've got a contribution record for this section, we can record it
                    // in the fragment's sparse array.
                    (*ca)[static_cast<size_t> (section)] = &v.back ();
                }
            }
        }
        return outputs;
    }

    // Simulating copy.
    //
    // In the real linker, we copy data to the output file and apply fixups as we go. In Layout,
    // we established the output sections an the contributions they carry. Each of those
    // contributions represents a particular section of a particular fragment.
    void copy (output_sections const & outputs) {
        for (auto const & l : outputs) {
            std::cout << "section: " << l.first << '\n';
            for (contribution const & c : l.second) {
                std::cout << "  " << *c.name << '\n';
                if (c.sections != nullptr) {
                    // Here show that we can reach the other contributions from the fragment that
                    // produced this contribution. This allows us to apply internal fixups for this
                    // section.
                    contribution_array const & sections = *c.sections;
                    for (auto const index : sections.get_indices ()) {
                        std::cout << "    " << *(sections[index]->name) << ':' << static_cast<section_kind> (index) << '\n';
                    }
                }
            }
            std::cout << '\n';
        }
    }

} // end anonymous namespace

int main () {
    chunked_sequence<uint8_t> storage;


auto * a = csalloc(&storage, 1, 4);
auto * b = csalloc(&storage, 1, 4);
assert (reinterpret_cast<uintptr_t> (a) % 4 == 0);
assert (reinterpret_cast<uintptr_t> (b) % 4 == 0);

    // First build some simulated fragments. Each has nothing more than an indication of the
    // different section types that it is carrying.
    std::vector<fragment> const fragments{
        fragment{"f1", {section_kind::text, section_kind::data}},
        fragment{"f2", {section_kind::text}},
        fragment{"f3",
                 {section_kind::text, section_kind::data, section_kind::read_only,
                  section_kind::mergeable_const_4}}};
    copy (layout (scan (&storage, fragments)));

    std::cout << "Used " << storage.size () << " bytes of storage for ifx links\n";
}
