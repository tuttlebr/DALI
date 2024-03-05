// Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "dali/pipeline/operator/error_reporting.h"
#include "dali/pipeline/operator/op_spec.h"

namespace dali {

std::vector<PythonStackFrame> GetOperatorOriginInfo(const OpSpec &spec) {
  auto origin_stack_filename = spec.GetRepeatedArgument<std::string>("_origin_stack_filename");
  auto origin_stack_lineno = spec.GetRepeatedArgument<int>("_origin_stack_lineno");
  auto origin_stack_name = spec.GetRepeatedArgument<std::string>("_origin_stack_name");
  auto origin_stack_line = spec.GetRepeatedArgument<std::string>("_origin_stack_line");

  std::vector<PythonStackFrame> origin_stack;
  origin_stack.reserve(origin_stack_filename.size());
  const char error[] = "Internal error, mismatch in origin stack trace data.";
  DALI_ENFORCE(origin_stack_filename.size() == origin_stack_lineno.size(), error);
  DALI_ENFORCE(origin_stack_filename.size() == origin_stack_name.size(), error);
  DALI_ENFORCE(origin_stack_filename.size() == origin_stack_line.size(), error);
  for (size_t i = 0; i < origin_stack_filename.size(); i++) {
    origin_stack.emplace_back(std::move(origin_stack_filename[i]), origin_stack_lineno[i],
                              std::move(origin_stack_name[i]), std::move(origin_stack_line[i]));
  }
  return origin_stack;
}


std::string FormatStack(const std::vector<PythonStackFrame> &stack_summary, bool include_context) {
  std::stringstream s;
  for (auto &frame_summary : stack_summary) {
    s << "  File \"" << frame_summary.filename << "\", line " << frame_summary.lineno << ", in "
      << frame_summary.name << "\n";
    if (include_context) {
      s << "    " << frame_summary.line << "\n";
    }
  }
  return s.str();
}

void PropagateError(ErrorInfo error) {
  try {
    if (error.exception) {
      std::rethrow_exception(error.exception);
    }
  }
  // DALI <-> Python mapped type errors:
  catch (DaliError &e) {
    if (error.context_info.size()) {
      e.UpdateMessage(make_string(error.context_info, e.what(), error.additional_message));
    }
    throw;
  }
  // Exceptions that are mapped by pybind from C++ into a sensible C++ one:
  catch (std::invalid_argument &e) {
    throw std::invalid_argument(
        make_string(error.context_info, e.what(), error.additional_message));
  } catch (std::domain_error &e) {
    throw std::domain_error(make_string(error.context_info, e.what(), error.additional_message));
  } catch (std::length_error &e) {
    throw std::length_error(make_string(error.context_info, e.what(), error.additional_message));
  } catch (std::out_of_range &e) {
    throw std::out_of_range(make_string(error.context_info, e.what(), error.additional_message));
  } catch (std::range_error &e) {
    throw std::range_error(make_string(error.context_info, e.what(), error.additional_message));
  }
  // Map the rest into runtime error (it would happen this way regardless)
  catch (std::exception &e) {
    throw std::runtime_error(make_string(error.context_info, e.what(), error.additional_message));
  } catch (...) {
    throw std::runtime_error(
        make_string(error.context_info, "Unknown critical error.", error.additional_message));
  }
}


}  // namespace dali