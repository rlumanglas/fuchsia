// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/async-loop/cpp/loop.h>
#include <lib/async-loop/default.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/syslog/cpp/macros.h>
#include <lib/zx/channel.h>
#include <zircon/types.h>

#include <cstdlib>
#include <string>

#include <fidl/test/compatibility/cpp/fidl.h>

#include "src/tests/fidl/compatibility/hlcpp_client_app.h"

namespace fidl {
namespace test {
namespace compatibility {

class EchoServerApp : public Echo {
 public:
  explicit EchoServerApp(async::Loop* loop)
      : loop_(loop), context_(sys::ComponentContext::CreateAndServeOutgoingDirectory()) {
    context_->outgoing()->AddPublicService(bindings_.GetHandler(this));
  }

  ~EchoServerApp() {}

  void EchoMinimal(std::string forward_to_server, EchoMinimalCallback callback) override {
    fidl::internal::HLCPPWireFormatV2Enabler enabler;
    if (!forward_to_server.empty()) {
      EchoClientApp app;
      bool failed = false;
      app.echo().set_error_handler([this, &forward_to_server, &failed](zx_status_t status) {
        failed = true;
        loop_->Quit();
        FX_LOGS(ERROR) << "error communicating with " << forward_to_server << ": " << status;
      });
      app.Start(forward_to_server);
      bool called_back = false;
      app.echo()->EchoMinimal("", [this, &called_back, &callback]() {
        called_back = true;
        callback();
        loop_->Quit();
      });
      while (!called_back && !failed) {
        loop_->Run();
      }
      loop_->ResetQuit();
    } else {
      callback();
    }
  }

  void EchoMinimalWithError(std::string forward_to_server, RespondWith result_variant,
                            EchoMinimalWithErrorCallback callback) override {
    fidl::internal::HLCPPWireFormatV2Enabler enabler;
    if (!forward_to_server.empty()) {
      EchoClientApp app;
      bool failed = false;
      app.echo().set_error_handler([this, &forward_to_server, &failed](zx_status_t status) {
        failed = true;
        loop_->Quit();
        FX_LOGS(ERROR) << "error communicating with " << forward_to_server << ": " << status;
      });
      app.Start(forward_to_server);
      bool called_back = false;
      app.echo()->EchoMinimalWithError(
          "", result_variant,
          [this, &called_back, &callback](Echo_EchoMinimalWithError_Result result) {
            fidl::internal::HLCPPWireFormatV2Enabler enabler;
            called_back = true;
            callback(std::move(result));
            loop_->Quit();
          });
      while (!called_back && !failed) {
        loop_->Run();
      }
      loop_->ResetQuit();
    } else {
      Echo_EchoMinimalWithError_Result result;
      if (result_variant == RespondWith::ERR) {
        result.set_err(0u);
      } else {
        result.set_response(Echo_EchoMinimalWithError_Response());
      }
      callback(std::move(result));
    }
  }

  void EchoMinimalNoRetVal(std::string forward_to_server) override {
    fidl::internal::HLCPPWireFormatV2Enabler enabler;
    if (!forward_to_server.empty()) {
      std::unique_ptr<EchoClientApp> app(new EchoClientApp);
      app->echo().set_error_handler([this, forward_to_server](zx_status_t status) {
        loop_->Quit();
        FX_LOGS(ERROR) << "error communicating with " << forward_to_server << ": " << status;
      });
      app->Start(forward_to_server);
      app->echo().events().EchoMinimalEvent = [this]() { this->HandleEchoMinimalEvent(); };
      app->echo()->EchoMinimalNoRetVal("");
      client_apps_.push_back(std::move(app));
    } else {
      for (const auto& binding : bindings_.bindings()) {
        binding->events().EchoMinimalEvent();
      }
    }
  }

  void EchoStruct(Struct value, std::string forward_to_server,
                  EchoStructCallback callback) override {
    fidl::internal::HLCPPWireFormatV2Enabler enabler;
    if (!forward_to_server.empty()) {
      EchoClientApp app;
      bool failed = false;
      app.echo().set_error_handler([this, &forward_to_server, &failed](zx_status_t status) {
        failed = true;
        loop_->Quit();
        FX_LOGS(ERROR) << "error communicating with " << forward_to_server << ": " << status;
      });
      app.Start(forward_to_server);
      bool called_back = false;
      app.echo()->EchoStruct(std::move(value), "", [this, &called_back, &callback](Struct resp) {
        fidl::internal::HLCPPWireFormatV2Enabler enabler;
        called_back = true;
        callback(std::move(resp));
        loop_->Quit();
      });
      while (!called_back && !failed) {
        loop_->Run();
      }
      loop_->ResetQuit();
    } else {
      callback(std::move(value));
    }
  }

  void EchoStructWithError(Struct value, default_enum err, std::string forward_to_server,
                           RespondWith result_variant,
                           EchoStructWithErrorCallback callback) override {
    fidl::internal::HLCPPWireFormatV2Enabler enabler;
    if (!forward_to_server.empty()) {
      EchoClientApp app;
      bool failed = false;
      app.echo().set_error_handler([this, &forward_to_server, &failed](zx_status_t status) {
        failed = true;
        loop_->Quit();
        FX_LOGS(ERROR) << "error communicating with " << forward_to_server << ": " << status;
      });
      app.Start(forward_to_server);
      bool called_back = false;
      app.echo()->EchoStructWithError(
          std::move(value), std::move(err), "", result_variant,
          [this, &called_back, &callback](Echo_EchoStructWithError_Result result) {
            fidl::internal::HLCPPWireFormatV2Enabler enabler;
            called_back = true;
            callback(std::move(result));
            loop_->Quit();
          });
      while (!called_back && !failed) {
        loop_->Run();
      }
      loop_->ResetQuit();
    } else {
      Echo_EchoStructWithError_Result result;
      if (result_variant == RespondWith::ERR) {
        result.set_err(err);
      } else {
        result.set_response(Echo_EchoStructWithError_Response(std::move(value)));
      }
      callback(std::move(result));
    }
  }

  void EchoStructNoRetVal(Struct value, std::string forward_to_server) override {
    fidl::internal::HLCPPWireFormatV2Enabler enabler;
    if (!forward_to_server.empty()) {
      std::unique_ptr<EchoClientApp> app(new EchoClientApp);
      app->echo().set_error_handler([this, forward_to_server](zx_status_t status) {
        loop_->Quit();
        FX_LOGS(ERROR) << "error communicating with " << forward_to_server << ": " << status;
      });
      app->Start(forward_to_server);
      app->echo().events().EchoEvent = [this](Struct resp) {
        this->HandleEchoEvent(std::move(resp));
      };
      app->echo()->EchoStructNoRetVal(std::move(value), "");
      client_apps_.push_back(std::move(app));
    } else {
      for (const auto& binding : bindings_.bindings()) {
        Struct to_send;
        value.Clone(&to_send);
        binding->events().EchoEvent(std::move(to_send));
      }
    }
  }

  void EchoArrays(ArraysStruct value, std::string forward_to_server,
                  EchoArraysCallback callback) override {
    fidl::internal::HLCPPWireFormatV2Enabler enabler;
    if (!forward_to_server.empty()) {
      EchoClientApp app;
      bool failed = false;
      app.echo().set_error_handler([this, &forward_to_server, &failed](zx_status_t status) {
        failed = true;
        loop_->Quit();
        FX_LOGS(ERROR) << "error communicating with " << forward_to_server << ": " << status;
      });
      app.Start(forward_to_server);
      bool called_back = false;
      app.echo()->EchoArrays(std::move(value), "",
                             [this, &called_back, &callback](ArraysStruct resp) {
                               fidl::internal::HLCPPWireFormatV2Enabler enabler;
                               called_back = true;
                               callback(std::move(resp));
                               loop_->Quit();
                             });
      while (!called_back && !failed) {
        loop_->Run();
      }
      loop_->ResetQuit();
    } else {
      callback(std::move(value));
    }
  }

  void EchoArraysWithError(ArraysStruct value, default_enum err, std::string forward_to_server,
                           RespondWith result_variant,
                           EchoArraysWithErrorCallback callback) override {
    fidl::internal::HLCPPWireFormatV2Enabler enabler;
    if (!forward_to_server.empty()) {
      EchoClientApp app;
      bool failed = false;
      app.echo().set_error_handler([this, &forward_to_server, &failed](zx_status_t status) {
        failed = true;
        loop_->Quit();
        FX_LOGS(ERROR) << "error communicating with " << forward_to_server << ": " << status;
      });
      app.Start(forward_to_server);
      bool called_back = false;
      app.echo()->EchoArraysWithError(
          std::move(value), std::move(err), "", result_variant,
          [this, &called_back, &callback](Echo_EchoArraysWithError_Result result) {
            fidl::internal::HLCPPWireFormatV2Enabler enabler;
            called_back = true;
            callback(std::move(result));
            loop_->Quit();
          });
      while (!called_back && !failed) {
        loop_->Run();
      }
      loop_->ResetQuit();
    } else {
      Echo_EchoArraysWithError_Result result;
      if (result_variant == RespondWith::ERR) {
        result.set_err(err);
      } else {
        result.set_response(Echo_EchoArraysWithError_Response(std::move(value)));
      }
      callback(std::move(result));
    }
  }

  void EchoVectors(VectorsStruct value, std::string forward_to_server,
                   EchoVectorsCallback callback) override {
    fidl::internal::HLCPPWireFormatV2Enabler enabler;
    if (!forward_to_server.empty()) {
      EchoClientApp app;
      bool failed = false;
      app.echo().set_error_handler([this, &forward_to_server, &failed](zx_status_t status) {
        failed = true;
        loop_->Quit();
        FX_LOGS(ERROR) << "error communicating with " << forward_to_server << ": " << status;
      });
      app.Start(forward_to_server);
      bool called_back = false;
      app.echo()->EchoVectors(std::move(value), "",
                              [this, &called_back, &callback](VectorsStruct resp) {
                                fidl::internal::HLCPPWireFormatV2Enabler enabler;
                                called_back = true;
                                callback(std::move(resp));
                                loop_->Quit();
                              });
      while (!called_back && !failed) {
        loop_->Run();
      }
      loop_->ResetQuit();
    } else {
      callback(std::move(value));
    }
  }

  void EchoVectorsWithError(VectorsStruct value, default_enum err, std::string forward_to_server,
                            RespondWith result_variant,
                            EchoVectorsWithErrorCallback callback) override {
    fidl::internal::HLCPPWireFormatV2Enabler enabler;
    if (!forward_to_server.empty()) {
      EchoClientApp app;
      bool failed = false;
      app.echo().set_error_handler([this, &forward_to_server, &failed](zx_status_t status) {
        failed = true;
        loop_->Quit();
        FX_LOGS(ERROR) << "error communicating with " << forward_to_server << ": " << status;
      });
      app.Start(forward_to_server);
      bool called_back = false;
      app.echo()->EchoVectorsWithError(
          std::move(value), std::move(err), "", result_variant,
          [this, &called_back, &callback](Echo_EchoVectorsWithError_Result result) {
            fidl::internal::HLCPPWireFormatV2Enabler enabler;
            called_back = true;
            callback(std::move(result));
            loop_->Quit();
          });
      while (!called_back && !failed) {
        loop_->Run();
      }
      loop_->ResetQuit();
    } else {
      Echo_EchoVectorsWithError_Result result;
      if (result_variant == RespondWith::ERR) {
        result.set_err(err);
      } else {
        result.set_response(Echo_EchoVectorsWithError_Response(std::move(value)));
      }
      callback(std::move(result));
    }
  }

  void EchoTable(AllTypesTable value, std::string forward_to_server,
                 EchoTableCallback callback) override {
    fidl::internal::HLCPPWireFormatV2Enabler enabler;
    if (!forward_to_server.empty()) {
      EchoClientApp app;
      bool failed = false;
      app.echo().set_error_handler([this, &forward_to_server, &failed](zx_status_t status) {
        failed = true;
        loop_->Quit();
        FX_LOGS(ERROR) << "error communicating with " << forward_to_server << ": " << status;
      });
      app.Start(forward_to_server);
      bool called_back = false;
      app.echo()->EchoTable(std::move(value), "",
                            [this, &called_back, &callback](AllTypesTable resp) {
                              fidl::internal::HLCPPWireFormatV2Enabler enabler;
                              called_back = true;
                              callback(std::move(resp));
                              loop_->Quit();
                            });
      while (!called_back && !failed) {
        loop_->Run();
      }
      loop_->ResetQuit();
    } else {
      callback(std::move(value));
    }
  }

  void EchoTableWithError(AllTypesTable value, default_enum err, std::string forward_to_server,
                          RespondWith result_variant,
                          EchoTableWithErrorCallback callback) override {
    fidl::internal::HLCPPWireFormatV2Enabler enabler;
    if (!forward_to_server.empty()) {
      EchoClientApp app;
      bool failed = false;
      app.echo().set_error_handler([this, &forward_to_server, &failed](zx_status_t status) {
        failed = true;
        loop_->Quit();
        FX_LOGS(ERROR) << "error communicating with " << forward_to_server << ": " << status;
      });
      app.Start(forward_to_server);
      bool called_back = false;
      app.echo()->EchoTableWithError(
          std::move(value), std::move(err), "", result_variant,
          [this, &called_back, &callback](Echo_EchoTableWithError_Result result) {
            fidl::internal::HLCPPWireFormatV2Enabler enabler;
            called_back = true;
            callback(std::move(result));
            loop_->Quit();
          });
      while (!called_back && !failed) {
        loop_->Run();
      }
      loop_->ResetQuit();
    } else {
      Echo_EchoTableWithError_Result result;
      if (result_variant == RespondWith::ERR) {
        result.set_err(err);
      } else {
        result.set_response(Echo_EchoTableWithError_Response(std::move(value)));
      }
      callback(std::move(result));
    }
  }

  void EchoXunions(std::vector<AllTypesXunion> value, std::string forward_to_server,
                   EchoXunionsCallback callback) override {
    fidl::internal::HLCPPWireFormatV2Enabler enabler;
    if (!forward_to_server.empty()) {
      EchoClientApp app;
      bool failed = false;
      app.echo().set_error_handler([this, &forward_to_server, &failed](zx_status_t status) {
        failed = true;
        loop_->Quit();
        FX_LOGS(ERROR) << "error communicating with " << forward_to_server << ": " << status;
      });
      app.Start(forward_to_server);
      bool called_back = false;
      app.echo()->EchoXunions(std::move(value), "",
                              [this, &called_back, &callback](std::vector<AllTypesXunion> resp) {
                                fidl::internal::HLCPPWireFormatV2Enabler enabler;
                                called_back = true;
                                callback(std::move(resp));
                                loop_->Quit();
                              });
      while (!called_back && !failed) {
        loop_->Run();
      }
      loop_->ResetQuit();
    } else {
      callback(std::move(value));
    }
  }

  void EchoXunionsWithError(std::vector<AllTypesXunion> value, default_enum err,
                            std::string forward_to_server, RespondWith result_variant,
                            EchoXunionsWithErrorCallback callback) override {
    fidl::internal::HLCPPWireFormatV2Enabler enabler;
    if (!forward_to_server.empty()) {
      EchoClientApp app;
      bool failed = false;
      app.echo().set_error_handler([this, &forward_to_server, &failed](zx_status_t status) {
        failed = true;
        loop_->Quit();
        FX_LOGS(ERROR) << "error communicating with " << forward_to_server << ": " << status;
      });
      app.Start(forward_to_server);
      bool called_back = false;
      app.echo()->EchoXunionsWithError(
          std::move(value), std::move(err), "", result_variant,
          [this, &called_back, &callback](Echo_EchoXunionsWithError_Result result) {
            fidl::internal::HLCPPWireFormatV2Enabler enabler;
            called_back = true;
            callback(std::move(result));
            loop_->Quit();
          });
      while (!called_back && !failed) {
        loop_->Run();
      }
      loop_->ResetQuit();
    } else {
      Echo_EchoXunionsWithError_Result result;
      if (result_variant == RespondWith::ERR) {
        result.set_err(err);
      } else {
        result.set_response(Echo_EchoXunionsWithError_Response(std::move(value)));
      }
      callback(std::move(result));
    }
  }

 private:
  void HandleEchoEvent(Struct value) {
    fidl::internal::HLCPPWireFormatV2Enabler enabler;
    for (const auto& binding : bindings_.bindings()) {
      Struct to_send;
      value.Clone(&to_send);
      binding->events().EchoEvent(std::move(to_send));
    }
  }
  void HandleEchoMinimalEvent() {
    fidl::internal::HLCPPWireFormatV2Enabler enabler;
    for (const auto& binding : bindings_.bindings()) {
      binding->events().EchoMinimalEvent();
    }
  }

  EchoPtr server_ptr;
  EchoServerApp(const EchoServerApp&) = delete;
  EchoServerApp& operator=(const EchoServerApp&) = delete;

  async::Loop* loop_;
  std::unique_ptr<sys::ComponentContext> context_;
  fidl::BindingSet<Echo> bindings_;
  std::vector<std::unique_ptr<EchoClientApp>> client_apps_;
};

}  // namespace compatibility
}  // namespace test
}  // namespace fidl

int main(int argc, const char** argv) {
  // The FIDL support lib requires async_get_default_dispatcher() to return
  // non-null.
  async::Loop loop(&kAsyncLoopConfigAttachToCurrentThread);

  fidl::test::compatibility::EchoServerApp app(&loop);
  loop.Run();
  return EXIT_SUCCESS;
}
