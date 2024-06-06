// Copyright 2023 Ant Group Co., Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstddef>
#include <utility>

#include "fmt/format.h"
#include "pybind11/complex.h"
#include "pybind11/functional.h"
#include "pybind11/iostream.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "yacl/base/exception.h"

#include "secretflow_serving/framework/model_info_processor.h"
#include "secretflow_serving/ops/graph.h"
#include "secretflow_serving/ops/graph_version.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

#include "secretflow_serving/protos/bundle.pb.h"
#include "secretflow_serving/protos/graph.pb.h"

namespace py = pybind11;

namespace secretflow::serving::op {

#define NO_GIL py::call_guard<py::gil_scoped_release>()

PYBIND11_MODULE(libserving, m) {
  m.doc() = R"pbdoc(
              Secretflow-Serving Python Library
                  )pbdoc";

  py::register_exception_translator(
      [](std::exception_ptr p) {  // NOLINT: pybind11
        try {
          if (p) {
            std::rethrow_exception(p);
          }
        } catch (const yacl::Exception& e) {
          // Translate this exception to a standard RuntimeError
          PyErr_SetString(PyExc_RuntimeError,
                          fmt::format("what: \n\t{}\nstacktrace: \n{}\n",
                                      e.what(), e.stack_trace())
                              .c_str());
        }
      });

  m.def("get_all_op_defs_impl", []() -> std::vector<py::bytes> {
    std::vector<py::bytes> result;
    auto op_defs = OpFactory::GetInstance()->GetAllOps();
    std::for_each(op_defs.begin(), op_defs.end(),
                  [&](const std::shared_ptr<const OpDef>& op) {
                    std::string content;
                    YACL_ENFORCE(op->SerializeToString(&content));
                    result.emplace_back(std::move(content));
                  });
    return result;
  });

  m.def(
      "graph_validator_impl",
      [](const py::bytes& graph_def_str) -> py::bytes {
        GraphDef graph_def;
        graph_def.ParseFromString(graph_def_str);
        Graph graph(graph_def);

        return graph_def_str;
      },
      py::arg("graph_def_str"));

  m.def(
      "get_graph_view_impl",
      [](const py::bytes& graph_def_str) -> py::bytes {
        GraphDef graph_def;
        graph_def.ParseFromString(graph_def_str);
        Graph graph(graph_def);
        auto view = graph.GetView();
        std::string view_str;
        YACL_ENFORCE(view.SerializeToString(&view_str));
        return view_str;
      },
      py::arg("graph_def_str"));

  m.def(
      "get_op_def_impl",
      [](const std::string& name) -> py::bytes {
        std::string result;
        const auto def = OpFactory::GetInstance()->Get(name);
        YACL_ENFORCE(def->SerializeToString(&result));
        return result;
      },
      py::arg("name"));

  m.def("get_graph_def_version_impl",
        []() -> std::string { return SERVING_GRAPH_VERSION_STRING; });

  m.def(
      "check_graph_view_impl",
      [](const std::map<std::string, std::string> graph_views) {
        YACL_ENFORCE(graph_views.size() > 1);
        auto iter = graph_views.begin();
        auto lcoal_party_id = iter->first;
        ModelInfo local_model_info;
        std::unordered_map<std::string, ModelInfo> remote_model_infos;
        local_model_info.mutable_graph_view()->ParseFromString(iter->second);
        iter++;
        while (iter != graph_views.end()) {
          ModelInfo model_info;
          model_info.mutable_graph_view()->ParseFromString(iter->second);
          remote_model_infos[iter->first] = model_info;
          iter++;
        }

        ModelInfoProcessor processor(lcoal_party_id, local_model_info,
                                     remote_model_infos);
      },
      py::arg("graph_views"));
}

}  // namespace secretflow::serving::op
