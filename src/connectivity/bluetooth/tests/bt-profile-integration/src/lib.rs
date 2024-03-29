// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use {
    anyhow::{format_err, Context, Error},
    bt_test_harness::{
        access::{expectation, AccessHarness},
        profile::ProfileHarness,
    },
    fidl::encoding::Decodable,
    fidl::endpoints::create_request_stream,
    fidl_fuchsia_bluetooth::{DeviceClass, ErrorCode, MAJOR_DEVICE_CLASS_MISCELLANEOUS},
    fidl_fuchsia_bluetooth_bredr::{
        ChannelParameters, ConnectParameters, ConnectionReceiverRequestStream, DataElement,
        L2capParameters, ProfileDescriptor, ProtocolDescriptor, ProtocolIdentifier,
        SearchResultsRequest, SearchResultsRequestStream, ServiceClassProfileIdentifier,
        ServiceDefinition, PSM_AVDTP,
    },
    fidl_fuchsia_bluetooth_sys::ProcedureTokenProxy,
    fidl_fuchsia_bluetooth_test::{BredrPeerParameters, HciEmulatorProxy, PeerProxy},
    fuchsia_async::{DurationExt, TimeoutExt},
    fuchsia_bluetooth::{
        constants::integration_timeout_duration,
        expectation::asynchronous::{ExpectableExt, ExpectableStateExt},
        types::{Address, PeerId, Uuid},
    },
    futures::{FutureExt, StreamExt, TryFutureExt},
};

/// This makes a custom BR/EDR service definition that runs over L2CAP.
fn service_definition_for_testing() -> ServiceDefinition {
    let test_uuid: Uuid = "f0c451a0-7e57-1111-2222-123456789ABC".parse().expect("UUID to parse");
    ServiceDefinition {
        service_class_uuids: Some(vec![test_uuid.into()]),
        protocol_descriptor_list: Some(vec![ProtocolDescriptor {
            protocol: ProtocolIdentifier::L2Cap,
            params: vec![DataElement::Uint16(0x100f)], // In the "dynamically-assigned" range
        }]),
        ..ServiceDefinition::new_empty()
    }
}

pub fn a2dp_sink_service_definition() -> ServiceDefinition {
    ServiceDefinition {
        service_class_uuids: Some(vec![Uuid::new16(0x110B).into()]), // Audio Sink UUID
        protocol_descriptor_list: Some(vec![
            ProtocolDescriptor {
                protocol: ProtocolIdentifier::L2Cap,
                params: vec![DataElement::Uint16(PSM_AVDTP)],
            },
            ProtocolDescriptor {
                protocol: ProtocolIdentifier::Avdtp,
                params: vec![DataElement::Uint16(0x0103)], // Indicate v1.3
            },
        ]),
        profile_descriptors: Some(vec![ProfileDescriptor {
            profile_id: ServiceClassProfileIdentifier::AdvancedAudioDistribution,
            major_version: 1,
            minor_version: 2,
        }]),
        ..ServiceDefinition::new_empty()
    }
}

fn add_service(profile: &ProfileHarness) -> Result<ConnectionReceiverRequestStream, anyhow::Error> {
    let service_defs = vec![service_definition_for_testing()];
    let (connect_client, connect_requests) =
        create_request_stream().context("ConnectionReceiver creation")?;

    let _ = profile.aux().profile.advertise(
        &mut service_defs.into_iter(),
        ChannelParameters::new_empty(),
        connect_client,
    );
    Ok(connect_requests)
}

async fn create_bredr_peer(proxy: &HciEmulatorProxy, address: Address) -> Result<PeerProxy, Error> {
    let peer_params = BredrPeerParameters {
        address: Some(address.into()),
        connectable: Some(true),
        device_class: Some(DeviceClass { value: MAJOR_DEVICE_CLASS_MISCELLANEOUS + 0 }),
        service_definition: Some(vec![a2dp_sink_service_definition()]),
        ..BredrPeerParameters::EMPTY
    };

    let (peer, remote) = fidl::endpoints::create_proxy()?;
    let _ = proxy
        .add_bredr_peer(peer_params, remote)
        .await?
        .map_err(|e| format_err!("Failed to register fake peer: {:#?}", e))?;
    Ok(peer)
}

async fn start_discovery(access: &AccessHarness) -> Result<ProcedureTokenProxy, Error> {
    // We create a capability to capture the discovery token, and pass it to the profile provider
    // Discovery will stop once we drop this token
    let (token, token_server) = fidl::endpoints::create_proxy()?;
    let fidl_response = access.aux().start_discovery(token_server);
    fidl_response
        .await?
        .map_err(|sys_err| format_err!("Error calling StartDiscovery(): {:?}", sys_err))?;
    Ok(token)
}

async fn add_search(
    profile: &ProfileHarness,
    profileid: ServiceClassProfileIdentifier,
) -> Result<SearchResultsRequestStream, Error> {
    let (results_client, results_stream) =
        create_request_stream().context("SearchResults creation")?;
    profile.aux().profile.search(profileid, &[], results_client)?;
    Ok(results_stream)
}

fn default_address() -> Address {
    Address::Public([1, 0, 0, 0, 0, 0])
}

#[test_harness::run_singlethreaded_test]
async fn test_add_profile(profile: ProfileHarness) -> Result<(), Error> {
    let _ = add_service(&profile)?;
    Ok(())
}

#[test_harness::run_singlethreaded_test]
async fn test_same_psm_twice_fails(profile: ProfileHarness) -> Result<(), Error> {
    let _request_stream = add_service(&profile)?;
    let mut second_request_stream = add_service(&profile)?;
    // Second request should have a closed stream
    match second_request_stream
        .next()
        .on_timeout(integration_timeout_duration().after_now(), || {
            Some(Err(fidl::Error::UnexpectedSyncResponse))
        })
        .await
    {
        None => Ok(()),
        x => Err(format_err!("Expected client to close, but instead got {:?}", x)),
    }
}

#[test_harness::run_singlethreaded_test]
async fn test_add_and_remove_profile(profile: ProfileHarness) -> Result<(), Error> {
    let request_stream = add_service(&profile)?;

    drop(request_stream);

    // Adding the profile a second time after removing it should succeed.
    let mut request_stream = add_service(&profile)?;

    // Request stream should be pending (otherwise it is an error)
    if request_stream.next().now_or_never().is_some() {
        return Err(format_err!("Should not have an error on re-adding the service"));
    }

    Ok(())
}

#[test_harness::run_singlethreaded_test]
async fn test_connect_unknown_peer(profile: ProfileHarness) -> Result<(), Error> {
    let fut = profile.aux().profile.connect(
        &mut PeerId(0xDEAD).into(),
        &mut ConnectParameters::L2cap(L2capParameters {
            psm: Some(PSM_AVDTP),
            ..L2capParameters::new_empty()
        }),
    );
    match fut.await {
        Ok(Err(ErrorCode::Failed)) => Ok(()),
        x => Err(format_err!("Expected error from connecting to an unknown peer, got {:?}", x)),
    }
}

// TODO(fxbug.dev/70927): Fix this and reenable
#[ignore]
#[test_harness::run_singlethreaded_test]
async fn test_add_search((access, profile): (AccessHarness, ProfileHarness)) -> Result<(), Error> {
    let emulator = profile.aux().emulator.clone();
    let peer_address = default_address();
    let profile_id = ServiceClassProfileIdentifier::AudioSink;
    let mut search_result = add_search(&profile, profile_id).await?;
    let _test_peer = create_bredr_peer(&emulator, peer_address).await?;
    let _discovery_result = start_discovery(&access).await?;

    let state = access
        .when_satisfied(
            expectation::peer_with_address(peer_address),
            integration_timeout_duration(),
        )
        .await?;

    let connected_peer_id = state.peers.values().find(|p| p.address == peer_address).unwrap().id;

    let fidl_response = access.aux().connect(&mut connected_peer_id.into());
    fidl_response
        .await?
        .map_err(|sys_err| format_err!("Error calling Connect(): {:?}", sys_err))?;
    let _ = access
        .when_satisfied(
            expectation::peer_connected(connected_peer_id, true),
            integration_timeout_duration(),
        )
        .await?;

    // The SDP search result conducted following connection should contain the
    // peer ID of the created peer.
    let service_found_fut = search_result.select_next_some().map_err(|e| format_err!("{:?}", e));
    let SearchResultsRequest::ServiceFound { peer_id, .. } = service_found_fut.await?;
    assert_eq!(connected_peer_id, peer_id.into());

    // Peer should be updated with discovered service.
    let _ = access
        .when_satisfied(
            expectation::peer_bredr_service_discovered(
                connected_peer_id,
                Uuid::new16(profile_id.into_primitive()),
            ),
            integration_timeout_duration(),
        )
        .await?;

    Ok(())
}

// TODO(fxbug.dev/1252): the rest of connect_l2cap tests (that actually succeed)
