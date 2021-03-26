// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
use diagnostics_data::LifecycleType;
use diagnostics_reader::{ArchiveReader, Lifecycle};
use fidl_fuchsia_diagnostics_persist::DataPersistenceMarker;
use fidl_fuchsia_sys::ComponentControllerEvent;
use fuchsia_async::futures::StreamExt;
use fuchsia_component::client::{launcher, App, AppBuilder};
use log::*;
use serde_json::{self, Value};
use std::fs::{create_dir, File};
use std::io::Read;
use std::{thread, time};

static METADATA_KEY: &str = "metadata";
static TIMESTAMP_KEY: &str = "timestamp";
static ELEPHANT_INJECTED_PATH: &str = "/cache";
static INJECTED_STORAGE_DIR: &str = "/tmp/injected_storage";

const ELEPHANT_URL: &str = "fuchsia-pkg://fuchsia.com/elephant-integration-tests#meta/elephant.cmx";
const INSPECT_PROVIDER_URL: &str =
    "fuchsia-pkg://fuchsia.com/elephant-integration-tests#meta/test_component.cmx";

const TEST_ELEPHANT_SERVICE_NAME: &str = "fuchsia.diagnostics.persist.DataPersistence_test-service";

/// Runs the elephant persistor and a test component that can have its inspect properties
/// manipulated by the test via fidl. then just trigger persistence on elephant.
#[fuchsia::test]
async fn event_count_sampler_test() {
    let _example_app = setup().await;

    wait_for_inspect_ready().await;

    let elephant_app = elephant().await;

    let elephant_service = elephant_app
        .connect_to_named_service::<DataPersistenceMarker>(TEST_ELEPHANT_SERVICE_NAME)
        .unwrap();

    elephant_service.persist("test-component-metric").await.unwrap();

    verify_elephant_files();

    // TODO(cphoenix)
    // kill the elephant service, and then restart it with an AppBuilder using the same dir you originally used, check to see that
    // the cache looks correct, re-persist, check again etc.
}

// Starts and returns an Elephant app. Assumes:
//  - The INJECTED_STORAGE_DIR has been created.
//  - No other Elephant app is running in that directory.
async fn elephant() -> App {
    AppBuilder::new(ELEPHANT_URL)
        .add_dir_to_namespace(
            ELEPHANT_INJECTED_PATH.into(),
            File::open(INJECTED_STORAGE_DIR).unwrap(),
        )
        .unwrap()
        .spawn(&launcher().unwrap())
        .unwrap()
}

async fn setup() -> App {
    create_dir(INJECTED_STORAGE_DIR).unwrap();

    let arguments = vec!["--rows=5".to_string(), "--columns=3".to_string()];

    let example_app =
        AppBuilder::new(INSPECT_PROVIDER_URL).args(arguments).spawn(&launcher().unwrap()).unwrap();

    let mut component_stream = example_app.controller().take_event_stream();

    match component_stream
        .next()
        .await
        .expect("component event stream has ended before termination event")
        .unwrap()
    {
        ComponentControllerEvent::OnDirectoryReady {} => {}
        ComponentControllerEvent::OnTerminated { return_code, termination_reason } => {
            panic!(
                "Component terminated unexpectedly. Code: {}. Reason: {:?}",
                return_code, termination_reason
            );
        }
    }

    example_app
}

async fn wait_for_inspect_ready() {
    loop {
        let results = ArchiveReader::new()
            .snapshot::<Lifecycle>()
            .await
            .unwrap()
            .into_iter()
            .filter(|e| {
                e.moniker.starts_with("test_component")
                    && e.metadata.lifecycle_event_type == LifecycleType::DiagnosticsReady
            })
            .collect::<Vec<_>>();

        if !results.is_empty() {
            break;
        }
    }
}

fn verify_elephant_files() {
    loop {
        let persisted_data_path =
            "/tmp/injected_storage/current/test-service/test-component-metric";
        let file = File::open(persisted_data_path);
        if file.is_err() {
            warn!("Persisted data file can't be opened (yet)");
            thread::sleep(time::Duration::from_secs(1));
            continue;
        }

        let mut contents = String::new();
        file.unwrap().read_to_string(&mut contents).unwrap();
        let result_json: Value = serde_json::from_str(&contents).expect("parsing json failed.");
        let mut string_result_array = result_json
            .as_array()
            .expect("result json is an array of objs.")
            .into_iter()
            .filter_map(|val| {
                let mut val = val.clone();

                val.as_object_mut().map(|obj: &mut serde_json::Map<String, serde_json::Value>| {
                    let metadata_obj = obj.get_mut(METADATA_KEY).unwrap().as_object_mut().unwrap();
                    metadata_obj.insert(TIMESTAMP_KEY.to_string(), serde_json::json!(0));
                    serde_json::to_string(&serde_json::to_value(obj).unwrap())
                        .expect("All entries in the array are valid.")
                })
            })
            .collect::<Vec<String>>();

        // Just because the file was present doesn't mean we persisted. Creation and
        // writing isn't atomic, and we sometimes flake as we race between the creation and write.
        if string_result_array.is_empty() {
            warn!("No data has been written to the persisted file (yet)");
            thread::sleep(time::Duration::from_secs(1));
            continue;
        }

        string_result_array.sort();

        let sorted_results_json_string = format!("[{}]", string_result_array.join(","));

        let sorted_results_json_struct: serde_json::Value =
            serde_json::from_str(&sorted_results_json_string).unwrap();

        let expected_result: Value = serde_json::from_str(&expected_text()).unwrap();

        assert_eq!(sorted_results_json_struct, expected_result);
        break;
    }
}

fn expected_text() -> String {
    r#"[
  {
    "data_source": "Inspect",
    "metadata": {
      "component_url": "fuchsia-pkg://fuchsia.com/elephant-integration-tests#meta/test_component.cmx",
      "errors": null,
      "filename": "fuchsia.inspect.Tree",
      "timestamp": 0
    },
    "moniker": "test_component.cmx",
    "payload": {
      "root": {
        "lazy-double": 3.14
      }
    },
    "version": 1
  },
  {
    "data_source": "Inspect",
    "metadata": {
      "component_url": "fuchsia-pkg://fuchsia.com/elephant-integration-tests#meta/test_component.cmx",
      "errors": null,
      "filename": "fuchsia.inspect.deprecated.Inspect",
      "timestamp": 0
    },
    "moniker": "test_component.cmx",
    "payload": {
      "root": {}
    },
    "version": 1
  },
  {
    "data_source": "Inspect",
    "metadata": {
      "component_url": "fuchsia-pkg://fuchsia.com/elephant-integration-tests#meta/test_component.cmx",
      "errors": null,
      "filename": "root.inspect",
      "timestamp": 0
    },
    "moniker": "test_component.cmx",
    "payload": {
      "root": {}
    },
    "version": 1
  }
]"#.to_string()
}
