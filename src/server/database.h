#ifndef SHROOM_SERVER_DATABASE_H
#define SHROOM_SERVER_DATABASE_H

#include <stdbool.h>

#include <sqlite3.h>

bool ShroomDatabaseInitializeSchema(sqlite3* db);
bool ShroomDatabaseSeedDefaults(sqlite3* db);

#endif
