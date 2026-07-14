#include "unity.h"

#include <string.h>

#include "../../tools/server_healthcheck.h"

void setUp(void) {}

void tearDown(void) {}

static ShroomServerProbeResponsePacket ValidResponse(void) {
  ShroomServerProbeResponsePacket packet = {
      .protocol_version = SHROOM_PROTOCOL_VERSION,
      .generation = SHROOM_HEALTHCHECK_GENERATION,
      .nonce = SHROOM_HEALTHCHECK_NONCE,
      .player_count = 12u,
      .capacity = SHROOM_SERVER_MAX_CLIENTS,
  };
  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_SERVER_PROBE_RESPONSE, sizeof(packet));
  return packet;
}

void test_config_defaults_and_overrides_are_bounded(void) {
  ShroomHealthcheckConfig config;
  char* defaults[] = {"healthcheck"};
  char* overrides[] = {"healthcheck",  "--host", "server.example", "--port", "9000",
                       "--timeout-ms", "3500"};

  TEST_ASSERT_TRUE(ShroomHealthcheckLoadConfig(&config, 1, defaults));
  TEST_ASSERT_EQUAL_STRING("127.0.0.1", config.host);
  TEST_ASSERT_EQUAL_UINT16(SHROOM_SERVER_PORT, config.port);
  TEST_ASSERT_EQUAL_UINT32(2000u, config.timeout_ms);

  TEST_ASSERT_TRUE(ShroomHealthcheckLoadConfig(&config, 7, overrides));
  TEST_ASSERT_EQUAL_STRING("server.example", config.host);
  TEST_ASSERT_EQUAL_UINT16(9000u, config.port);
  TEST_ASSERT_EQUAL_UINT32(3500u, config.timeout_ms);
}

void test_config_rejects_invalid_or_incomplete_arguments(void) {
  ShroomHealthcheckConfig config;
  char* zero_port[] = {"healthcheck", "--port", "0"};
  char* short_timeout[] = {"healthcheck", "--timeout-ms", "99"};
  char* missing_value[] = {"healthcheck", "--host"};
  char* unknown[] = {"healthcheck", "--unknown", "value"};

  TEST_ASSERT_FALSE(ShroomHealthcheckLoadConfig(&config, 3, zero_port));
  TEST_ASSERT_FALSE(ShroomHealthcheckLoadConfig(&config, 3, short_timeout));
  TEST_ASSERT_FALSE(ShroomHealthcheckLoadConfig(&config, 2, missing_value));
  TEST_ASSERT_FALSE(ShroomHealthcheckLoadConfig(&config, 3, unknown));
}

void test_valid_probe_response_is_accepted(void) {
  ShroomServerProbeResponsePacket packet = ValidResponse();
  const ShroomServerProbeResponsePacket* response = NULL;

  TEST_ASSERT_TRUE(ShroomHealthcheckValidateResponse(
      SHROOM_ENET_CHANNEL_CONTROL, (const uint8_t*)&packet, sizeof(packet), &response));
  TEST_ASSERT_EQUAL_PTR(&packet, response);
}

void test_wrong_channel_size_nonce_and_capacity_are_rejected(void) {
  ShroomServerProbeResponsePacket packet = ValidResponse();
  const ShroomServerProbeResponsePacket* response = NULL;

  TEST_ASSERT_FALSE(ShroomHealthcheckValidateResponse(
      SHROOM_ENET_CHANNEL_INPUT, (const uint8_t*)&packet, sizeof(packet), &response));
  TEST_ASSERT_FALSE(ShroomHealthcheckValidateResponse(
      SHROOM_ENET_CHANNEL_CONTROL, (const uint8_t*)&packet, sizeof(packet) - 1u, &response));
  packet.nonce += 1u;
  TEST_ASSERT_FALSE(ShroomHealthcheckValidateResponse(
      SHROOM_ENET_CHANNEL_CONTROL, (const uint8_t*)&packet, sizeof(packet), &response));
  packet = ValidResponse();
  packet.player_count = packet.capacity + 1u;
  TEST_ASSERT_FALSE(ShroomHealthcheckValidateResponse(
      SHROOM_ENET_CHANNEL_CONTROL, (const uint8_t*)&packet, sizeof(packet), &response));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_config_defaults_and_overrides_are_bounded);
  RUN_TEST(test_config_rejects_invalid_or_incomplete_arguments);
  RUN_TEST(test_valid_probe_response_is_accepted);
  RUN_TEST(test_wrong_channel_size_nonce_and_capacity_are_rejected);
  return UNITY_END();
}
