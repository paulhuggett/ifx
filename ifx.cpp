// Standard library
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "pstore/mcrepo/section_sparray.hpp"
#include "cs_alloc.hpp"

using namespace pstore;
using repo::section_kind;

struct contribution;
using contribution_sparray = repo::section_sparray<contribution const *>;

struct contribution {
    constexpr contribution (section_kind skind, std::string const * const name,
                            contribution_sparray const * const sections) noexcept
            : skind{skind}
            , name{name}
            , sections{sections} {}
    section_kind const skind;
    std::string const * const name;
    contribution_sparray const * const sections;
};

// A super-minimal simulated fragment data structure.
struct fragment {
    explicit fragment (std::string name, std::initializer_list<section_kind> sections) noexcept
            : name{std::move (name)}
            , sections{sections} {}
    // Fragments don't really have names. Here I use the name to show that the code is finding the
    // correct fragment.
    std::string const name;
    // In the real implementation, this is a sparse array where the indices are the section numbers
    // and the values are the offset to the start of the payload data associated with that section.
    std::set<section_kind> const sections;
};



namespace {

    // A function to be used as unique_ptr<> deleter which simply does nothing.
    constexpr void nop_free (void *) noexcept {}

    // A type similar to unique_ptr<> but where placement new is used for allocation and therefore
    // the deleter function is a no-op.
    template <typename T>
    using placement_unique_ptr = std::unique_ptr<T, decltype (&nop_free)>;

    template <class T, class... Args>
    std::enable_if_t<!std::is_array<T>::value, placement_unique_ptr<T>>
    make_placement_unique_ptr (void * ptr, Args &&... args) {
        return placement_unique_ptr<T>{new (ptr) T (std::forward<Args> (args)...), &nop_free};
    }

} // end anonymous namespace



namespace {

    // scan
    // ~~~~
    // Simulating the linker's scan phase.
    //
    // Here we go through a compilation's definitions. Each name is entered into the symbol
    // table and, if it is being kept, we resolve all of the external fixups using the symbol
    // table. The final step is to allocate storage for a pointer to each output section
    // contribution: there is one of these records per section in the fragment.
    using fragment_to_contribution_map =
        std::map<fragment const *, placement_unique_ptr<contribution_sparray>>;

    fragment_to_contribution_map scan (chunked_sequence<std::uint8_t> * const storage,
                                       std::vector<fragment> const & fragments) {

        // Allocates an instance of contribution_sparray<> for fragment f.
        auto const create = [&] (fragment const & f) {
            // A fragment with only a single section cannot, by definition, have any internal
            // fixups to process since their purpose is to allow one section of a fragment to
            // reference another. That means that we don't need to allocate any storage for this
            // fragment.
            auto const num_sections = f.sections.size ();
            if (num_sections < 2U) {
                return placement_unique_ptr<contribution_sparray>{nullptr, &nop_free};
            }

            auto * const ptr = cs_alloc (storage, contribution_sparray::size_bytes (num_sections), alignof (contribution_sparray));
            assert (reinterpret_cast<std::uintptr_t> (ptr) % alignof (contribution_sparray) == 0 && "Storage must be aligned correctly");
            return make_placement_unique_ptr<contribution_sparray> (ptr, std::begin (f.sections), std::end (f.sections));
        };

        fragment_to_contribution_map fc;
        for (fragment const & f : fragments) {
            fc.emplace (&f, create (f));
        }
        return fc;
    }

    // layout
    // ~~~~~~
    // Simulating the linker's layout phase.
    //
    // Layout assigns each section of each fragment to a specific output section. This is
    // recorded as a "contribution" which holds the associated address in target memory.
    using output_sections = std::map<section_kind, chunked_sequence<contribution>>;

    output_sections layout (fragment_to_contribution_map const & captr) {
        output_sections outputs;

        for (auto const & c : captr) {
            auto * const f = std::get<fragment const * const> (c);
            auto const & ca = std::get<placement_unique_ptr<contribution_sparray>> (c);

            for (section_kind const section : f->sections) {
                auto & v = outputs[section];
                v.emplace_back (section, &f->name, ca.get ()); // build a contribution entry
                if (ca) {
                    // Now that we've got a contribution record for this section, we can record it
                    // in the fragment's sparse array.
                    assert (ca->has_index (section) && "The array does have an index for one of the fragment's sections");
                    (*ca)[section] = &v.back ();
                }
            }
        }
        return outputs;
    }

    // copy
    // ~~~~
    // Simulating the linker's copy phase.
    //
    // In the real linker, we copy data to the output file and apply fixups as we go. In Layout,
    // we established the output sections and the contributions they carry. Each of those
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
                    contribution_sparray const & sections = *c.sections;
                    for (section_kind const index : sections.get_indices ()) {
                        std::cout << "    " << *(sections[index]->name) << ':' << index << '\n';
                    }
                }
            }
            std::cout << '\n';
        }
    }

} // end anonymous namespace

int main () {
    chunked_sequence<std::uint8_t> storage;

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
