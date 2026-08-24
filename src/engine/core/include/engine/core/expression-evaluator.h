#pragma once

#include "eval-context.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// ExpressionEvaluator: Lightweight, sandboxed expression parser.
//
// Responsibilities:
// - Parse and evaluate simple property expressions (e.g. "entity.health < 20")
// - Support property access (e.g. "player.strength"), arithmetic, comparisons
// - No loops, no function definitions, no side effects
// - Used by conditions, actions, dialog nodes, triggers
//
// Key Invariants:
// - All expressions are read-only (no state mutation)
// - Expressions must complete in O(n) time (n = expression length)
// - Expressions must use only whitelisted properties (security)
// - Invalid expressions return nullopt (safe failure)
// - Type coercion is consistent (number, string, bool)
//
// Thread Safety:
// - evaluate: safe from any thread (no state mutation)
// - Property resolution depends on EvalContext (may reference entity ECS)
// ============================================================================

// Expression value types (simplified)
using ExpressionValue = std::variant<double,  // Numbers (int coerced to double)
                                     std::string,  // Strings
                                     bool          // Booleans
                                     >;

class ExpressionEvaluator {
public:
  // Evaluate expression string in context
  // expr: expression text (e.g. "entity.health < 20")
  // ctx: evaluation context (provides property values)
  // Returns: result value or nullopt if parse/eval error
  std::optional<ExpressionValue> evaluate(std::string_view expr,
                                          const EvalContext& ctx) const;

  // Validate expression syntax without evaluating
  // Returns: true if expression is valid; false if parse error
  bool isValid(std::string_view expr) const;

  // Get last error message (if evaluate returned nullopt)
  std::string_view lastError() const;

  // Whitelist/blacklist properties for security (game layer configures)
  void allowProperty(std::string_view property_name);
  void denyProperty(std::string_view property_name);

private:
  /// Parse the RHS operand and apply the binary operator to lhs.
  std::optional<ExpressionValue>
  parseRhsAndApply(std::string_view expr, size_t& pos,
                   const ExpressionValue& lhs) const;

  /// Apply a binary operator to two expression values.
  std::optional<ExpressionValue>
  applyBinaryOp(const ExpressionValue& lhs, std::string_view op,
                const ExpressionValue& rhs) const;

  /// Parse a full expression: literal, or literal op literal.
  std::optional<ExpressionValue> parseExpression(std::string_view expr,
                                                 size_t& pos) const;

  /// Last error message from a failed evaluate() call (mutable because error
  /// reporting is a diagnostic side-channel that does not affect evaluation).
  mutable std::string last_error_;
};

}  // namespace eng
