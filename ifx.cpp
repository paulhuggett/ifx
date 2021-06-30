// Standard library
#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "contribution_sparray.hpp"
#include "cs_alloc.hpp"

struct contribution {
    constexpr contribution (pstore::repo::section_kind skind, std::string const * const name,
                            contribution_sparray const * sections) noexcept
            : skind{skind}
            , name{name}
            , sections{sections} {}
    pstore::repo::section_kind const skind;
    std::string const * const name;
    contribution_sparray const * const sections;
};

// Our super-minimal simulated fragment data structure.
struct fragment {
    explicit fragment (std::string name,
                       std::initializer_list<pstore::repo::section_kind> sections) noexcept
            : name{std::move (name)}
            , sections{sections} {}
    // Fragments don't really have names. Here I use the name to show that the code is finding the
    // correct fragment.
    std::string const name;
    // In the real implementation, this is a sparse array where the indices are the section numbers
    // and the values are the offset to the start of the payload data associated with that section.
    std::set<pstore::repo::section_kind> const sections;
};

using namespace pstore;
using repo::section_kind;

using output_sections = std::map<section_kind, chunked_sequence<contribution>>;
using fragment_to_contribution_map = std::map<fragment const *, contribution_sparray *>;

namespace {
    // scan
    // ~~~~
    // Simulating the linker's scan phase.
    //
    // Here we go through a compilation's definitions. Each name is entered into the symbol
    // table and, if it is being kept, we resolve all of the external fixups using the symbol
    // table. The final step is to allocate storage for a pointer to each output section
    // contribution: there is one of these records per section in the fragment.
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
                auto * const ptr =
                    cs_alloc (storage, contribution_sparray::size_bytes (num_sections),
                              alignof (contribution_sparray));
                assert (reinterpret_cast<uintptr_t> (ptr) % alignof (contribution_sparray) == 0 &&
                        "Storage must be aligned correctly");
                captr[&f] =
                    new (ptr) contribution_sparray (std::begin (f.sections), std::end (f.sections));
            }
        }

        return captr;
    }

    // layout
    // ~~~~~~
    // Simulating the linker's layout phase.
    //
    // Layout assigns each section of each fragment to a specific output section. This is
    // recorded as a "contribution" which holds the associated address in target memory.
    output_sections
    layout (fragment_to_contribution_map const & captr) {
        output_sections outputs;

        for (auto const & c: captr) {
            auto * const f = std::get<fragment const * const> (c);
            auto * const ca = std::get<contribution_sparray *> (c);

            for (section_kind const section : f->sections) {
                auto & v = outputs[section];
                v.emplace_back (section, &f->name, ca); // build a contribution entry
                if (ca != nullptr) {
                    // Now that we've got a contribution record for this section, we can record it
                    // in the fragment's sparse array.
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
                    contribution_sparray const & sections = *c.sections;
                    for (section_kind const index : sections.get_indices ()) {
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
