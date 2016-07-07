// Copyright 2016 MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <mongo_odm/config/prelude.hpp>

#include <cstddef>
#include <stdexcept>
#include <tuple>
#include <type_traits>

#include <bsoncxx/builder/core.hpp>
#include <bsoncxx/view_or_value.hpp>

#include <mongo_odm/nvp.hpp>

namespace mongo_odm {
MONGO_ODM_INLINE_NAMESPACE_BEGIN

/**
 * Represents a binary comparison expression between a key and a value. e.g. (User.age > 21).
 */
template <typename Base, typename T>
class ComparisonExpr {
   public:
    /**
     * Constructs a query expression for the given key, value, and comparison type
     * @param  nvp           A name-value pair corresponding to a key in a document
     * @param  field         The value that the key is being compared to.
     * @param  selector_type The type of comparison operator, such at gt (>) or ne (!=).
     */
    constexpr ComparisonExpr(const Nvp<Base, T> &nvp, T field, const char *selector_type)
        : nvp(nvp), field(std::move(field)), selector_type(selector_type) {
    }

    const Nvp<Base, T> &nvp;
    T field;
    const char *selector_type;

    /**
     * Appends this expression to a BSON core builder, as a key-value pair of the form
     * "key: {$cmp: val}", where $cmp is some comparison operator.
     * @param builder A BSON core builder
     */
    void append_to_bson(bsoncxx::builder::core &bson) const {
        bson.key_view(nvp.name);
        bson.open_document();
        bson.key_view(selector_type);
        bson.append(field);
        bson.close_document();
    }

    /**
     * Converts the expression to a BSON filter for a query.
     * The resulting BSON is of the form "{key: {$cmp: val}}".
     */
    operator bsoncxx::document::view_or_value() const {
        auto bson = bsoncxx::builder::core(false);
        append_to_bson(bson);
        return bsoncxx::document::view_or_value(bson.extract_document());
    }
};

/**
 * This represents an expression with the $not operator, which wraps a comparison expression and
 * negates it.
 */
template <typename Base, typename T>
class NotExpr {
   public:
    /**
     * Creates a $not expression taht negates the given comparison expression.
     * @param  expr A comparison expression
     */
    constexpr NotExpr(const ComparisonExpr<Base, T> &expr) : expr(expr) {
    }

    /**
     * Appends this expression to a BSON core builder,
     * as a key-value pair of the form "key: {$not: {$cmp: val}}".
     * @param builder a BSON core builder
     */
    void append_to_bson(bsoncxx::builder::core &bson) const {
        bson.key_view(expr.nvp.name);
        bson.open_document();
        bson.key_view("$not");
        bson.open_document();
        bson.key_view(expr.selector_type);
        bson.append(expr.field);
        bson.close_document();
        bson.close_document();
    }

    /**
     * Converts the expression to a BSON filter for a query.
     * The format of the BSON is "{key: {$not: {$cmp: val}}}".
     */
    operator bsoncxx::document::view_or_value() const {
        auto bson = bsoncxx::builder::core(false);
        append_to_bson(bson);
        return bsoncxx::document::view_or_value(bson.extract_document());
    }

    const ComparisonExpr<Base, T> expr;
};

template <typename Head, typename Tail>
class ExpressionList {
   public:
    /**
     * Constructs an expression list.
     * @param head  The first element in the list
     * @param tail  The remainder of the list
     */
    constexpr ExpressionList(const Head &head, const Tail &tail) : head(head), tail(tail) {
    }

    /**
     * Appends this expression list to the given core builder by appending the first expression
     * in the list, and then recursing on the rest of the list.
     * @param builder A basic BSON core builder.
     */
    void append_to_bson(bsoncxx::builder::core &bson) const {
        head.append_to_bson(bson);
        tail.append_to_bson(bson);
    }

    /**
     * Casts the expression list to a BSON query of  the form { expr1, expr2, ....}
     */
    operator bsoncxx::document::view_or_value() const {
        auto bson = bsoncxx::builder::core(false);
        append_to_bson(bson);
        return bsoncxx::document::view_or_value(bson.extract_document());
    }

    const Head head;
    const Tail tail;
};

template <typename Expr1, typename Expr2>
class BooleanExpr {
   public:
    /**
     * Constructs a boolean expression from two other expressions, and a certain operator.
     * @param  lhs The left-hand side of the expression.
     * @param  rhs The right-hand side of the expression.
     * @param  op  The operator of the expression, e.g. AND or OR.
     */
    constexpr BooleanExpr(const Expr1 &lhs, const Expr2 &rhs, const char *op)
        : lhs(lhs), rhs(rhs), op(op) {
    }

    /**
     * Appends this query to a BSON core builder as a key-value pair "$op: [{lhs}, {rhs}]"
     * @param builder A basic BSON core builder.
     */
    void append_to_bson(bsoncxx::builder::core &bson) const {
        bson.key_view(op);
        bson.open_array();

        // append left hand side
        bson.open_document();
        lhs.append_to_bson(bson);
        bson.close_document();

        // append right hand side
        bson.open_document();
        rhs.append_to_bson(bson);
        bson.close_document();

        bson.close_array();
    }

    /**
     * Converts the expression to a BSON filter for a query,
     * in the form "{ $op: [{lhs}, {rhs}] }"
     */
    operator bsoncxx::document::view_or_value() const {
        auto bson = bsoncxx::builder::core(false);
        append_to_bson(bson);
        return bsoncxx::document::view_or_value(bson.extract_document());
    }

    const Expr1 lhs;
    const Expr2 rhs;
    const char *op;
};

/* A templated struct that holds a boolean value.
* This value is TRUE for types that are query expression,
* and FALSE for all other types.
*/
template <typename>
struct is_expression {
    constexpr static bool value = false;
};

template <typename Base, typename T>
struct is_expression<ComparisonExpr<Base, T>> {
    constexpr static bool value = true;
};

template <typename Base, typename T>
struct is_expression<NotExpr<Base, T>> {
    constexpr static bool value = true;
};

template <typename Head, typename Tail>
struct is_expression<ExpressionList<Head, Tail>> {
    constexpr static bool value = true;
};

template <typename Expr1, typename Expr2>
struct is_expression<BooleanExpr<Expr1, Expr2>> {
    constexpr static bool value = true;
};

/* Operator overloads for creating and combining expressions */

/* Overload comparison operators for name-value pairs to create expressions.
 * Separate operators are defined for bool types to prevent confusing implicit casting to bool
 * for non-bool types.
 */
template <typename Base, typename T, typename U,
          typename = typename std::enable_if_t<!std::is_same<T, bool>::value>>
constexpr ComparisonExpr<Base, T> operator==(const Nvp<Base, T> &lhs, const U &rhs) {
    return ComparisonExpr<Base, T>(lhs, rhs, "$eq");
}

template <typename Base, typename T,
          typename = typename std::enable_if_t<std::is_same<T, bool>::value>>
constexpr ComparisonExpr<Base, T> operator==(const Nvp<Base, T> &lhs, const T &rhs) {
    return ComparisonExpr<Base, T>(lhs, rhs, "$eq");
}

template <typename Base, typename T, typename U,
          typename = typename std::enable_if_t<!std::is_same<T, bool>::value>>
constexpr ComparisonExpr<Base, T> operator>(const Nvp<Base, T> &lhs, const U &rhs) {
    return ComparisonExpr<Base, T>(lhs, rhs, "$gt");
}

template <typename Base, typename T,
          typename = typename std::enable_if_t<std::is_same<T, bool>::value>>
constexpr ComparisonExpr<Base, T> operator>(const Nvp<Base, T> &lhs, const T &rhs) {
    return ComparisonExpr<Base, T>(lhs, rhs, "$gt");
}

template <typename Base, typename T, typename U,
          typename = typename std::enable_if_t<!std::is_same<T, bool>::value>>
constexpr ComparisonExpr<Base, T> operator>=(const Nvp<Base, T> &lhs, const U &rhs) {
    return ComparisonExpr<Base, T>(lhs, rhs, "$gte");
}

template <typename Base, typename T,
          typename = typename std::enable_if_t<std::is_same<T, bool>::value>>
constexpr ComparisonExpr<Base, T> operator>=(const Nvp<Base, T> &lhs, const T &rhs) {
    return ComparisonExpr<Base, T>(lhs, rhs, "$gte");
}

template <typename Base, typename T, typename U,
          typename = typename std::enable_if_t<!std::is_same<T, bool>::value>>
constexpr ComparisonExpr<Base, T> operator<(const Nvp<Base, T> &lhs, const U &rhs) {
    return ComparisonExpr<Base, T>(lhs, rhs, "$lt");
}

template <typename Base, typename T,
          typename = typename std::enable_if_t<std::is_same<T, bool>::value>>
constexpr ComparisonExpr<Base, T> operator<(const Nvp<Base, T> &lhs, const T &rhs) {
    return ComparisonExpr<Base, T>(lhs, rhs, "$lt");
}

template <typename Base, typename T, typename U,
          typename = typename std::enable_if_t<!std::is_same<T, bool>::value>>
constexpr ComparisonExpr<Base, T> operator<=(const Nvp<Base, T> &lhs, const U &rhs) {
    return ComparisonExpr<Base, T>(lhs, rhs, "$lte");
}

template <typename Base, typename T,
          typename = typename std::enable_if_t<std::is_same<T, bool>::value>>
constexpr ComparisonExpr<Base, T> operator<=(const Nvp<Base, T> &lhs, const T &rhs) {
    return ComparisonExpr<Base, T>(lhs, rhs, "$lte");
}

template <typename Base, typename T, typename U,
          typename = typename std::enable_if_t<!std::is_same<T, bool>::value>>
constexpr ComparisonExpr<Base, T> operator!=(const Nvp<Base, T> &lhs, const U &rhs) {
    return ComparisonExpr<Base, T>(lhs, rhs, "$ne");
}

template <typename Base, typename T,
          typename = typename std::enable_if_t<std::is_same<T, bool>::value>>
constexpr ComparisonExpr<Base, T> operator!=(const Nvp<Base, T> &lhs, const T &rhs) {
    return ComparisonExpr<Base, T>(lhs, rhs, "$ne");
}

/**
 * Negates a comparison expression in a $not expression.
 */
template <typename Base, typename T>
constexpr NotExpr<Base, T> operator!(const ComparisonExpr<Base, T> &expr) {
    return NotExpr<Base, T>(expr);
}

/**
 * Comma operator that combines two expressions or an expression and an expression list into a new
 * expression list.
 */
template <typename Head, typename Tail,
          typename = typename std::enable_if<is_expression<Head>::value &&
                                             is_expression<Tail>::value>::type>
constexpr ExpressionList<Head, Tail> operator,(Tail &&tail, Head &&head) {
    return ExpressionList<Head, Tail>(head, tail);
}

/**
 * Boolean operator overloads for expressions.
 */
template <typename Expr1, typename Expr2,
          typename = typename std::enable_if<is_expression<Expr1>::value &&
                                             is_expression<Expr2>::value>::type>
constexpr BooleanExpr<Expr1, Expr2> operator&&(const Expr1 &lhs, const Expr2 &rhs) {
    return BooleanExpr<Expr1, Expr2>(lhs, rhs, "$and");
}

template <typename Expr1, typename Expr2,
          typename = typename std::enable_if<is_expression<Expr1>::value &&
                                             is_expression<Expr2>::value>::type>
constexpr BooleanExpr<Expr1, Expr2> operator||(const Expr1 &lhs, const Expr2 &rhs) {
    return BooleanExpr<Expr1, Expr2>(lhs, rhs, "$or");
}

MONGO_ODM_INLINE_NAMESPACE_END
}  // namespace bson_mapper

#include <mongo_odm/config/postlude.hpp>