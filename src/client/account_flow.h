#ifndef SHROOM_CLIENT_ACCOUNT_FLOW_H
#define SHROOM_CLIENT_ACCOUNT_FLOW_H

#include <stdatomic.h>

#include "client_rest.h"

typedef enum ShroomAccountFlowState {
  SHROOM_ACCOUNT_FLOW_IDLE = 0,
  SHROOM_ACCOUNT_FLOW_WORKING,
  SHROOM_ACCOUNT_FLOW_SUCCEEDED,
  SHROOM_ACCOUNT_FLOW_FAILED,
} ShroomAccountFlowState;

typedef enum ShroomAccountFlowOperation {
  SHROOM_ACCOUNT_OPERATION_NONE = 0,
  SHROOM_ACCOUNT_OPERATION_RESTORE,
  SHROOM_ACCOUNT_OPERATION_REGISTER,
  SHROOM_ACCOUNT_OPERATION_LOGIN,
  SHROOM_ACCOUNT_OPERATION_GET_ME,
  SHROOM_ACCOUNT_OPERATION_LOGOUT,
} ShroomAccountFlowOperation;

typedef struct ShroomAccountFlow {
  ShroomClientRest* rest;
  void* thread_handle;
  atomic_int state;
  ShroomAccountFlowOperation operation;
  ShroomClientRestResult result;
  char identity[SHROOM_CLIENT_REST_EMAIL_MAX + 1u];
  char username[SHROOM_CLIENT_REST_USERNAME_MAX + 1u];
  char email[SHROOM_CLIENT_REST_EMAIL_MAX + 1u];
  char password[129];
} ShroomAccountFlow;

void ShroomAccountFlowInit(ShroomAccountFlow* flow, ShroomClientRest* rest);
void ShroomAccountFlowShutdown(ShroomAccountFlow* flow);
ShroomAccountFlowState ShroomAccountFlowPoll(ShroomAccountFlow* flow);
void ShroomAccountFlowAcknowledge(ShroomAccountFlow* flow);
bool ShroomAccountFlowStartRestore(ShroomAccountFlow* flow);
bool ShroomAccountFlowStartRegister(ShroomAccountFlow* flow, const char* username,
                                    const char* email, const char* password);
bool ShroomAccountFlowStartLogin(ShroomAccountFlow* flow, const char* identity,
                                 const char* password);
bool ShroomAccountFlowStartGetMe(ShroomAccountFlow* flow);
bool ShroomAccountFlowStartLogout(ShroomAccountFlow* flow);

#endif
