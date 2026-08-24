#include <cctype>
#include <cstdlib>
#include <engine/core/expression-evaluator.h>

namespace eng {

namespace {

  /// Skip whitespace.
  void skipWs(std::string_view expr, size_t& pos) {
    while (pos < expr.size() &&
           std::isspace(static_cast<unsigned char>(expr[pos])) != 0) {
      ++pos;
    }
  }

  /// Try to parse a boolean literal.
  std::optional<bool> parseBool(std::string_view expr, size_t& pos) {
    if (expr.substr(pos, 4) == "true") {
      pos += 4;
      return true;
    }
    if (expr.substr(pos, 5) == "false") {
      pos += 5;
      return false;
    }
    return std::nullopt;
  }

  /// Try to parse a string literal.
  std::optional<std::string> parseString(std::string_view expr, size_t& pos) {
    if (pos >= expr.size() || expr[pos] != '"') {
      return std::nullopt;
    }
    ++pos;
    std::string result;
    while (pos < expr.size() && expr[pos] != '"') {
      result += expr[pos++];
    }
    if (pos >= expr.size()) {
      return std::nullopt;
    }
    ++pos;
    return result;
  }

  /// Try to parse a numeric literal using strtod.
  std::optional<double> parseNumber(std::string_view expr, size_t& pos) {
    const char* start = expr.data() + pos;
    char* end = nullptr;
    const double value = std::strtod(start, &end);
    if (end == start) {
      return std::nullopt;
    }
    pos = static_cast<size_t>(end - expr.data());
    return value;
  }

  /// Try to parse a string literal, setting error on unterminated input.
  std::optional<ExpressionValue> tryParseStringLiteral(std::string_view expr,
                                                       size_t& pos,
                                                       std::string& error) {
    if (pos >= expr.size() || expr[pos] != '"') {
      return std::nullopt;
    }
    if (auto s = parseString(expr, pos)) {
      return ExpressionValue(std::move(*s));
    }
    error = "unterminated string literal";
    return std::nullopt;
  }

  /// Try to parse a single literal value (bool, string, or number).
  std::optional<ExpressionValue>
  tryParseLiteral(std::string_view expr, size_t& pos, std::string& error) {
    if (auto b = parseBool(expr, pos)) {
      return ExpressionValue(*b);
    }
    if (auto s = tryParseStringLiteral(expr, pos, error)) {
      return s;
    }
    if (!error.empty()) {
      return std::nullopt;
    }
    if (auto n = parseNumber(expr, pos)) {
      return ExpressionValue(*n);
    }
    error = "unrecognized expression";
    return std::nullopt;
  }

  /// Try to parse a two-character operator (>=, <=, ==, !=).
  std::optional<std::string> parseTwoCharOp(std::string_view expr,
                                            size_t& pos) {
    if (pos + 1 >= expr.size()) {
      return std::nullopt;
    }
    auto two = expr.substr(pos, 2);
    if (two == ">=" || two == "<=" || two == "==" || two == "!=") {
      pos += 2;
      return std::string(two);
    }
    return std::nullopt;
  }

  /// Try to parse a single-character operator (+, -, *, /, >, <).
  std::optional<std::string> parseSingleCharOp(std::string_view expr,
                                               size_t& pos) {
    char ch = expr[pos];
    if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '>' ||
        ch == '<') {
      ++pos;
      return std::string(1, ch);
    }
    return std::nullopt;
  }

  /// Try to parse a binary operator (+, -, *, /, >, <, >=, <=, ==, !=).
  std::optional<std::string> parseOperator(std::string_view expr, size_t& pos) {
    if (pos >= expr.size()) {
      return std::nullopt;
    }
    if (auto two = parseTwoCharOp(expr, pos)) {
      return two;
    }
    return parseSingleCharOp(expr, pos);
  }

  /// Evaluate a binary arithmetic operation.
  std::optional<ExpressionValue> evalArithmetic(double lhs, std::string_view op,
                                                double rhs) {
    if (op == "+") {
      return ExpressionValue(lhs + rhs);
    }
    if (op == "-") {
      return ExpressionValue(lhs - rhs);
    }
    if (op == "*") {
      return ExpressionValue(lhs * rhs);
    }
    if (op == "/") {
      return ExpressionValue(lhs / rhs);
    }
    return std::nullopt;
  }

  /// Evaluate a binary comparison operation.
  std::optional<ExpressionValue> evalComparison(
      double lhs, std::string_view op,
      // Algorithm: Map comparison operator string to bool ExpressionValue.
      double rhs) {
    if (op == ">") {
      return ExpressionValue(lhs > rhs);
    }
    if (op == "<") {
      return ExpressionValue(lhs < rhs);
    }
    if (op == ">=") {
      return ExpressionValue(lhs >= rhs);
    }
    if (op == "<=") {
      return ExpressionValue(lhs <= rhs);
    }
    if (op == "==") {
      return ExpressionValue(lhs == rhs);
    }
    if (op == "!=") {
      return ExpressionValue(lhs != rhs);
    }
    return std::nullopt;
  }

}  // namespace

/// Parse the right-hand side operand and operator from a binary expression.
std::optional<ExpressionValue>
ExpressionEvaluator::parseRhsAndApply(std::string_view expr, size_t& pos,
                                      const ExpressionValue& lhs) const {
  auto op = parseOperator(expr, pos);
  if (!op) {
    last_error_ = "expected operator";
    return std::nullopt;
  }
  skipWs(expr, pos);
  auto rhs_val = tryParseLiteral(expr, pos, last_error_);
  if (!rhs_val) {
    return std::nullopt;
  }
  return applyBinaryOp(lhs, *op, *rhs_val);
}

/// Apply a binary operator to two expression values.
std::optional<ExpressionValue>
ExpressionEvaluator::applyBinaryOp(const ExpressionValue& lhs,
                                   std::string_view op,
                                   const ExpressionValue& rhs) const {
  if (!std::holds_alternative<double>(lhs) ||
      !std::holds_alternative<double>(rhs)) {
    last_error_ = "operands must be numeric";
    return std::nullopt;
  }
  double lhs_d = std::get<double>(lhs);
  double rhs_d = std::get<double>(rhs);
  auto result = evalArithmetic(lhs_d, op, rhs_d);
  if (!result) {
    result = evalComparison(lhs_d, op, rhs_d);
  }
  if (!result) {
    last_error_ = "unknown operator";
  }
  return result;
}

/// Parse a full expression: literal, or literal op literal.
std::optional<ExpressionValue>
ExpressionEvaluator::parseExpression(std::string_view expr, size_t& pos) const {
  auto lhs = tryParseLiteral(expr, pos, last_error_);
  if (!lhs) {
    return std::nullopt;
  }
  skipWs(expr, pos);
  if (pos >= expr.size()) {
    return lhs;
  }
  return parseRhsAndApply(expr, pos, *lhs);
}

std::optional<ExpressionValue>
ExpressionEvaluator::evaluate(std::string_view expr,
                              const EvalContext& /*ctx*/) const {
  last_error_.clear();
  size_t pos = 0;
  skipWs(expr, pos);
  if (pos >= expr.size()) {
    last_error_ = "empty expression";
    return std::nullopt;
  }
  return parseExpression(expr, pos);
}

bool ExpressionEvaluator::isValid(std::string_view expr) const {
  EvalContext dummy{};
  return evaluate(expr, dummy).has_value();
}

std::string_view ExpressionEvaluator::lastError() const {
  return last_error_;
}

void ExpressionEvaluator::allowProperty(std::string_view /*property_name*/) {
  // Placeholder: property allow-list tracking
}

void ExpressionEvaluator::denyProperty(std::string_view /*property_name*/) {
  // Placeholder: property deny-list tracking
}

}  // namespace eng
