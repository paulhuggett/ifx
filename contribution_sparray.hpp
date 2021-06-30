//
//  contribution_sparray.hpp
//  ifx
//
//  Created by Paul Bowen-Huggett on 30/06/2021.
//

#ifndef CONTRIBUTION_SPARRAY_HPP
#define CONTRIBUTION_SPARRAY_HPP

#include "pstore/adt/sparse_array.hpp"
#include "pstore/mcrepo/section.hpp"


/// Derives the correct type for the bitmap field of a sparse_array<> given the maximum index value.
template <uintmax_t V, typename Enable = void>
struct sparray_bitmap {};
template <uintmax_t V>
struct sparray_bitmap<V, typename std::enable_if_t<(V <= 8U)>> {
    using type = uint8_t;
};
template <uintmax_t V>
struct sparray_bitmap<V, typename std::enable_if_t<(V > 8U && V <= 16U)>> {
    using type = uint16_t;
};
template <uintmax_t V>
struct sparray_bitmap<V, typename std::enable_if_t<(V > 16U && V <= 32U)>> {
    using type = uint32_t;
};
template <uintmax_t V>
struct sparray_bitmap<V, typename std::enable_if_t<(V > 32U && V <= 64U)>> {
    using type = uint64_t;
};

template <uintmax_t V>
using sparray_bitmap_t = typename sparray_bitmap<V>::type;


struct contribution;

/// Returns the maximum section-kind value.
constexpr decltype (auto) max_section_kind () noexcept {
    using section_kind = pstore::repo::section_kind;
    return static_cast<std::underlying_type_t<section_kind>> (section_kind::last);
}

/// A wrapper for the pstore sparse_array<> type which is specialized for indices of type
/// pstore::repo::section_kind.
class contribution_sparray {
    using section_kind = pstore::repo::section_kind;
    using index_type = std::underlying_type_t<section_kind>;
    using array_type = pstore::sparse_array<contribution *, sparray_bitmap_t<max_section_kind ()>>;

public:
    using value_type = array_type::value_type;

    template <typename Iterator>
    contribution_sparray (Iterator first, Iterator last)
            : sa_{first, last} {}

    value_type & operator[] (section_kind k) { return sa_[static_cast<index_type> (k)]; }
    value_type const & operator[] (section_kind k) const {
        return sa_[static_cast<index_type> (k)];
    }

    static size_t size_bytes (size_t num_sections) noexcept {
        return array_type::size_bytes (num_sections);
    }

    class indices {
    public:
        class const_iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = section_kind;
            using difference_type = array_type::indices::const_iterator::difference_type;
            using pointer = value_type const *;
            using reference = value_type const &;

            constexpr explicit const_iterator (array_type::indices::const_iterator it) noexcept
                    : it_{it}
                    , pos_{static_cast<section_kind> (*it)} {}

            constexpr bool operator== (const_iterator const & rhs) const noexcept {
                return it_ == rhs.it_;
            }
            constexpr bool operator!= (const_iterator const & rhs) const noexcept {
                return !operator== (rhs);
            }

            constexpr reference operator* () const noexcept { return pos_; }

            const_iterator & operator++ () noexcept { // prefix++
                ++it_;
                pos_ = static_cast<section_kind> (*it_);
                return *this;
            }

            const_iterator operator++ (int) noexcept { // postfix++
                auto prev = *this;
                ++(*this);
                return prev;
            }

            const_iterator operator+ (unsigned x) const { return const_iterator{it_ + x}; }

        private:
            array_type::indices::const_iterator it_;
            section_kind pos_;
        };
        constexpr explicit indices (array_type::indices const & i)
                : i_{i} {}
        constexpr const_iterator begin () const noexcept { return const_iterator{i_.begin ()}; }
        constexpr const_iterator end () const noexcept { return const_iterator{i_.end ()}; }
        constexpr bool empty () const noexcept { return i_.empty (); }

    private:
        array_type::indices const i_;
    };

    indices get_indices () const noexcept { return indices{sa_.get_indices ()}; }

private:
    array_type sa_;
};

#endif // CONTRIBUTION_SPARRAY_HPP
