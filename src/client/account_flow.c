#include "account_flow.h"

#include <stdlib.h>
#include <string.h>

#include <openssl/crypto.h>

#if defined(_WIN32)
#include <process.h>
#include <windows.h>
#else
#include <pthread.h>
#endif

static void ClearRequest(ShroomAccountFlow* flow) {
  OPENSSL_cleanse(flow->identity, sizeof(flow->identity));
  OPENSSL_cleanse(flow->username, sizeof(flow->username));
  OPENSSL_cleanse(flow->email, sizeof(flow->email));
  OPENSSL_cleanse(flow->password, sizeof(flow->password));
}

static int RunOperation(ShroomAccountFlow* flow) {
  switch (flow->operation) {
  case SHROOM_ACCOUNT_OPERATION_RESTORE:
    flow->result = ShroomClientRestRestore(flow->rest);
    break;
  case SHROOM_ACCOUNT_OPERATION_REGISTER:
    flow->result =
        ShroomClientRestRegister(flow->rest, flow->username, flow->email, flow->password);
    break;
  case SHROOM_ACCOUNT_OPERATION_LOGIN:
    flow->result = ShroomClientRestLogin(flow->rest, flow->identity, flow->password);
    break;
  case SHROOM_ACCOUNT_OPERATION_GET_ME:
    flow->result = ShroomClientRestGetMe(flow->rest);
    break;
  case SHROOM_ACCOUNT_OPERATION_LOGOUT:
    flow->result = ShroomClientRestLogout(flow->rest);
    break;
  case SHROOM_ACCOUNT_OPERATION_NONE:
  default:
    flow->result = SHROOM_CLIENT_REST_INVALID_ARGUMENT;
    break;
  }
  ClearRequest(flow);
  if ((flow->result == SHROOM_CLIENT_REST_OK) &&
      ((flow->operation == SHROOM_ACCOUNT_OPERATION_RESTORE) ||
       (flow->operation == SHROOM_ACCOUNT_OPERATION_REGISTER) ||
       (flow->operation == SHROOM_ACCOUNT_OPERATION_LOGIN))) {
    flow->result = ShroomClientRestGetMe(flow->rest);
  }
  atomic_store_explicit(&flow->state,
                        flow->result == SHROOM_CLIENT_REST_OK ? SHROOM_ACCOUNT_FLOW_SUCCEEDED
                                                              : SHROOM_ACCOUNT_FLOW_FAILED,
                        memory_order_release);
  return 0;
}

#if defined(_WIN32)
static unsigned __stdcall RunThread(void* context) { return (unsigned)RunOperation(context); }

static bool StartThread(ShroomAccountFlow* flow) {
  const uintptr_t handle = _beginthreadex(NULL, 0u, RunThread, flow, 0u, NULL);
  flow->thread_handle = handle != 0u ? (void*)handle : NULL;
  return handle != 0u;
}

static void JoinThread(ShroomAccountFlow* flow) {
  if (flow->thread_handle != NULL) {
    WaitForSingleObject((HANDLE)flow->thread_handle, INFINITE);
    CloseHandle((HANDLE)flow->thread_handle);
    flow->thread_handle = NULL;
  }
}
#else
typedef struct ShroomAccountPosixThread {
  pthread_t id;
} ShroomAccountPosixThread;

static void* RunThread(void* context) {
  (void)RunOperation(context);
  return NULL;
}

static bool StartThread(ShroomAccountFlow* flow) {
  ShroomAccountPosixThread* handle = malloc(sizeof(*handle));

  if (handle == NULL) {
    return false;
  }
  if (pthread_create(&handle->id, NULL, RunThread, flow) != 0) {
    free(handle);
    return false;
  }
  flow->thread_handle = handle;
  return true;
}

static void JoinThread(ShroomAccountFlow* flow) {
  if (flow->thread_handle != NULL) {
    ShroomAccountPosixThread* handle = flow->thread_handle;
    (void)pthread_join(handle->id, NULL);
    free(handle);
    flow->thread_handle = NULL;
  }
}
#endif

void ShroomAccountFlowInit(ShroomAccountFlow* flow, ShroomClientRest* rest) {
  if (flow == NULL) {
    return;
  }
  memset(flow, 0, sizeof(*flow));
  flow->rest = rest;
  atomic_init(&flow->state, SHROOM_ACCOUNT_FLOW_IDLE);
}

void ShroomAccountFlowShutdown(ShroomAccountFlow* flow) {
  if (flow == NULL) {
    return;
  }
  JoinThread(flow);
  ClearRequest(flow);
  flow->operation = SHROOM_ACCOUNT_OPERATION_NONE;
  atomic_store_explicit(&flow->state, SHROOM_ACCOUNT_FLOW_IDLE, memory_order_release);
}

ShroomAccountFlowState ShroomAccountFlowPoll(ShroomAccountFlow* flow) {
  ShroomAccountFlowState state;

  if (flow == NULL) {
    return SHROOM_ACCOUNT_FLOW_FAILED;
  }
  state = (ShroomAccountFlowState)atomic_load_explicit(&flow->state, memory_order_acquire);
  if ((state == SHROOM_ACCOUNT_FLOW_SUCCEEDED) || (state == SHROOM_ACCOUNT_FLOW_FAILED)) {
    JoinThread(flow);
  }
  return state;
}

void ShroomAccountFlowAcknowledge(ShroomAccountFlow* flow) {
  if (flow == NULL) {
    return;
  }
  if (ShroomAccountFlowPoll(flow) != SHROOM_ACCOUNT_FLOW_WORKING) {
    flow->operation = SHROOM_ACCOUNT_OPERATION_NONE;
    atomic_store_explicit(&flow->state, SHROOM_ACCOUNT_FLOW_IDLE, memory_order_release);
  }
}

static bool Start(ShroomAccountFlow* flow, ShroomAccountFlowOperation operation) {
  if ((flow == NULL) || (flow->rest == NULL) ||
      (ShroomAccountFlowPoll(flow) == SHROOM_ACCOUNT_FLOW_WORKING)) {
    return false;
  }
  JoinThread(flow);
  flow->operation = operation;
  flow->result = SHROOM_CLIENT_REST_OK;
  atomic_store_explicit(&flow->state, SHROOM_ACCOUNT_FLOW_WORKING, memory_order_release);
  if (!StartThread(flow)) {
    flow->result = SHROOM_CLIENT_REST_TRANSPORT_ERROR;
    atomic_store_explicit(&flow->state, SHROOM_ACCOUNT_FLOW_FAILED, memory_order_release);
    return false;
  }
  return true;
}

static bool CopyField(char* destination, size_t capacity, const char* source) {
  const size_t length = source != NULL ? strlen(source) : 0u;

  if ((length == 0u) || (length >= capacity)) {
    return false;
  }
  memcpy(destination, source, length + 1u);
  return true;
}

bool ShroomAccountFlowStartRestore(ShroomAccountFlow* flow) {
  return Start(flow, SHROOM_ACCOUNT_OPERATION_RESTORE);
}

bool ShroomAccountFlowStartRegister(ShroomAccountFlow* flow, const char* username,
                                    const char* email, const char* password) {
  if ((flow == NULL) || (ShroomAccountFlowPoll(flow) == SHROOM_ACCOUNT_FLOW_WORKING)) {
    return false;
  }
  if (!CopyField(flow->username, sizeof(flow->username), username) ||
      !CopyField(flow->email, sizeof(flow->email), email) ||
      !CopyField(flow->password, sizeof(flow->password), password)) {
    ClearRequest(flow);
    return false;
  }
  if (!Start(flow, SHROOM_ACCOUNT_OPERATION_REGISTER)) {
    ClearRequest(flow);
    return false;
  }
  return true;
}

bool ShroomAccountFlowStartLogin(ShroomAccountFlow* flow, const char* identity,
                                 const char* password) {
  if ((flow == NULL) || (ShroomAccountFlowPoll(flow) == SHROOM_ACCOUNT_FLOW_WORKING)) {
    return false;
  }
  if (!CopyField(flow->identity, sizeof(flow->identity), identity) ||
      !CopyField(flow->password, sizeof(flow->password), password)) {
    ClearRequest(flow);
    return false;
  }
  if (!Start(flow, SHROOM_ACCOUNT_OPERATION_LOGIN)) {
    ClearRequest(flow);
    return false;
  }
  return true;
}

bool ShroomAccountFlowStartLogout(ShroomAccountFlow* flow) {
  return Start(flow, SHROOM_ACCOUNT_OPERATION_LOGOUT);
}

bool ShroomAccountFlowStartGetMe(ShroomAccountFlow* flow) {
  return Start(flow, SHROOM_ACCOUNT_OPERATION_GET_ME);
}
