#ifndef SIMDJSON_INTERNAL_DOM_PARSER_IMPLEMENTATION_H
#define SIMDJSON_INTERNAL_DOM_PARSER_IMPLEMENTATION_H

#include "simdjson/common_defs.h"
#include "simdjson/error.h"
#include <memory>

namespace simdjson {

namespace dom {
class document;
} // namespace dom

namespace internal {

// expectation: sizeof(scope_descriptor) = 64/8.
struct scope_descriptor {
  uint32_t tape_index; // where, on the tape, does the scope ([,{) begins
  uint32_t count; // how many elements in the scope
}; // struct scope_descriptor

#ifdef SIMDJSON_USE_COMPUTED_GOTO
typedef void* ret_address;
#else
typedef char ret_address;
#endif

/**
 * An implementation of simdjson's DOM parser for a particular CPU architecture.
 *
 * This class is expected to be accessed only by pointer, and never move in memory (though the
 * pointer can move).
 */
class dom_parser_implementation {
public:

  /**
   * @private For internal implementation use
   *
   * Run a full JSON parse on a single document (stage1 + stage2).
   * 
   * Guaranteed only to be called when capacity > document length.
   *
   * Overridden by each implementation.
   *
   * @param buf The json document to parse. *MUST* be allocated up to len + SIMDJSON_PADDING bytes.
   * @param len The length of the json document.
   * @return The error code, or SUCCESS if there was no error.
   */
  WARN_UNUSED virtual error_code parse(const uint8_t *buf, size_t len, dom::document &doc) noexcept = 0;

  /**
   * @private For internal implementation use
   *
   * Stage 1 of the document parser.
   * 
   * Guaranteed only to be called when capacity > document length.
   *
   * Overridden by each implementation.
   *
   * @param buf The json document to parse.
   * @param len The length of the json document.
   * @param streaming Whether this is being called by parser::parse_many.
   * @return The error code, or SUCCESS if there was no error.
   */
  WARN_UNUSED virtual error_code stage1(const uint8_t *buf, size_t len, bool streaming) noexcept = 0;

  /**
   * @private For internal implementation use
   *
   * Stage 2 of the document parser.
   * 
   * Called after stage1().
   *
   * Overridden by each implementation.
   *
   * @param doc The document to output to.
   * @return The error code, or SUCCESS if there was no error.
   */
  WARN_UNUSED virtual error_code stage2(dom::document &doc) noexcept = 0;

  /**
   * @private For internal implementation use
   *
   * Stage 2 of the document parser for parser::parse_many.
   *
   * Guaranteed only to be called after stage1(), with buf and len being a subset of the total stage1 buf/len.
   * Overridden by each implementation.
   *
   * @param buf The json document to parse.
   * @param len The length of the json document.
   * @param doc The document to output to.
   * @param next_json The next structural index. Start this at 0 the first time, and it will be updated to the next value to pass each time.
   * @return The error code, SUCCESS if there was no error, or SUCCESS_AND_HAS_MORE if there was no error and stage2 can be called again.
   */
  WARN_UNUSED virtual error_code stage2(const uint8_t *buf, size_t len, dom::document &doc, size_t &next_json) noexcept = 0;

  /**
   * Change the capacity of this parser.
   * 
   * Generally used for reallocation.
   *
   * @param capacity The new capacity.
   * @param max_depth The new max_depth.
   * @return The error code, or SUCCESS if there was no error.
   */
  virtual error_code set_capacity(size_t capacity) noexcept = 0;

  /**
   * Change the max depth of this parser.
   *
   * Generally used for reallocation.
   *
   * @param capacity The new capacity.
   * @param max_depth The new max_depth.
   * @return The error code, or SUCCESS if there was no error.
   */
  virtual error_code set_max_depth(size_t max_depth) noexcept = 0;

  /**
   * Deallocate this parser.
   */
  virtual ~dom_parser_implementation() = default;

  /** Next location to write to in the tape */
  uint32_t current_loc{0};

  /** Number of structural indices passed from stage 1 to stage 2 */
  uint32_t n_structural_indexes{0};
  /** Structural indices passed from stage 1 to stage 2 */
  std::unique_ptr<uint32_t[]> structural_indexes{};

  /** Tape location of each open { or [ */
  std::unique_ptr<internal::scope_descriptor[]> containing_scope{};

  /** Return address of each open { or [ */
  std::unique_ptr<internal::ret_address[]> ret_address{};

  /** Error code, used ENTIRELY to make gcc not be slower than before. Not actually consumed. */
  error_code error{UNINITIALIZED};

  /**
   * The largest document this parser can support without reallocating.
   *
   * @return Current capacity, in bytes.
   */
  really_inline size_t capacity() const noexcept;

  /**
   * The maximum level of nested object and arrays supported by this parser.
   *
   * @return Maximum depth, in bytes.
   */
  really_inline size_t max_depth() const noexcept;

  /**
   * Ensure this parser has enough memory to process JSON documents up to `capacity` bytes in length
   * and `max_depth` depth.
   *
   * @param capacity The new capacity.
   * @param max_depth The new max_depth. Defaults to DEFAULT_MAX_DEPTH.
   * @return The error, if there is one.
   */
  WARN_UNUSED inline error_code allocate(size_t capacity, size_t max_depth) noexcept;

protected:
  /**
   * The maximum document length this parser supports.
   *
   * Buffers are large enough to handle any document up to this length.
   */
  size_t _capacity{0};

  /**
   * The maximum depth (number of nested objects and arrays) supported by this parser.
   *
   * Defaults to DEFAULT_MAX_DEPTH.
   */
  size_t _max_depth{0};
}; // class dom_parser_implementation

really_inline size_t dom_parser_implementation::capacity() const noexcept {
  return _capacity;
}

really_inline size_t dom_parser_implementation::max_depth() const noexcept {
  return _max_depth;
}

WARN_UNUSED
inline error_code dom_parser_implementation::allocate(size_t capacity, size_t max_depth) noexcept {
  if (this->max_depth() != max_depth) {
    error_code err = set_max_depth(max_depth);
    if (err) { return err; }
  }
  if (_capacity != capacity) {
    error_code err = set_capacity(capacity);
    if (err) { return err; }
  }
  return SUCCESS;
}

} // namespace internal
} // namespace simdjson

#endif // SIMDJSON_INTERNAL_DOM_PARSER_IMPLEMENTATION_H
