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

#include "catch.hpp"

#include <boson/bson_streambuf.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/types.hpp>

#include <iostream>

using namespace boson;

/**
 * An object that compares a given BSON document to an original reference.
 * This is used for unit testing.
 */
class doc_validator {
   public:
    doc_validator(bsoncxx::document::value doc) : _count(0), _doc(doc) {
    }

    // Getter for the number of documents compared so far.
    int count() {
        return _count;
    }

    /**
     * This asserts that the given document is the same as the reference document.
     */
    void check(bsoncxx::document::value v) {
        _count++;
        REQUIRE(v.view() == _doc.view());
    }

   private:
    // Keeps track of the number of documents compared by this validator.
    int _count;
    bsoncxx::document::value _doc;
};

TEST_CASE("bson_output_streambuf can faithfully transfer a document",
          "[boson::bson_output_streambuf]") {
    // set up test BSON document
    std::string json_str = R"({"a": 1, "b":[1,2,3], "c": {"a": 1}})";
    auto bson_obj = bsoncxx::from_json(json_str);
    auto bson_view = bson_obj.view();
    const uint8_t *data = bson_view.data();
    int len = bson_view.length();

    // set up document validator
    doc_validator validator = doc_validator(bson_obj);

    // set up stream. The callback passes created documents into the validator object.
    bson_output_streambuf b_buff([&validator](bsoncxx::document::value v) { validator.check(v); });
    std::ostream doc_stream(&b_buff);

    // write document to stream
    doc_stream.write(reinterpret_cast<const char *>(data), len);
    // write document second time
    doc_stream.write(reinterpret_cast<const char *>(data), len);
    REQUIRE(validator.count() == 2);
}

TEST_CASE("char_array_streambuf properly implements seeking.", "[boson::bson_input_streambuf]") {
    size_t len = 100;
    char *data = new char[len];
    char_array_streambuf buffer(data, len);
    std::istream is(&buffer);

    // Seek through the buffer
    is.seekg(0, is.beg);
    REQUIRE(is.tellg() == 0);
    is.seekg(0, is.end);
    REQUIRE(is.tellg() == static_cast<std::istream::pos_type>(len));

    is.seekg(50, is.beg);
    REQUIRE(is.tellg() == 50);
    is.seekg(-20, is.end);
    REQUIRE(is.tellg() == static_cast<std::istream::pos_type>(len - 20));

    // seek relative to current position
    std::streampos current_pos = is.tellg();
    is.seekg(10, is.cur);
    REQUIRE(is.tellg() == (int)current_pos + 10);

    // seeking cannot go past buffer
    is.seekg(len + 100);
    REQUIRE(is.tellg() == static_cast<std::istream::pos_type>(len));
}

TEST_CASE("bson_input_streambuf can faithfully send over the bytes of a BSON document",
          "[boson::bson_input_streambuf]") {
    // set up test BSON document
    std::string json_str = R"({"a": 1, "b":[1,2,3], "c": {"a": 1}})";
    auto bson_obj = bsoncxx::from_json(json_str);
    auto bson_view = bson_obj.view();
    const uint8_t *data = bson_view.data();
    int len = bson_view.length();

    // set up istream
    bson_istream bis(bson_view);

    // read bytes of BSON document
    uint8_t *new_data = new uint8_t[len];
    bis.read(reinterpret_cast<char *>(new_data), len);
    auto new_doc_view = bsoncxx::document::view(new_data, len);
    REQUIRE(bson_view == new_doc_view);
    // read one more byte, should reach EOF
    bis.ignore(1);
    REQUIRE(bis.eof());
}
