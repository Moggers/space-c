#ifndef GAME_CONTRACTS
#define GAME_CONTRACTS

#include <assert.h>
#include <stdint.h>

#include "./entities.h"

#define MAX_CONTRACTS 1024
#define MAX_CONTRACT_XLIST 4

#define CONTRACT_ORE 1

char *CONTRACT_NAME[MAX_CONTRACTS];
uint32_t CONTRACT_AMOUNT[MAX_CONTRACTS];
uint32_t CONTRACT_COUNT = 1; // Not actually count.. We skip the first one;
uint32_t CONTRACT_CLAIMING_ENTITYID[MAX_CONTRACTS];
uint32_t CONTRACT_ISSUING_ENTITY[MAX_CONTRACTS];
uint32_t CONTRACT_TYPE[MAX_CONTRACTS];

void contract_add(uint32_t entityId, uint32_t contract_type, char *contractname,
                  uint32_t amount) {
  uint32_t newContract = 0;
  for (newContract = 0; newContract < MAX_CONTRACTS_PER_ENTITY; newContract++) {
    if (ENTITY_CONTRACTS[newContract][newContract] == 0) {
      break;
    }
  }
  CONTRACT_NAME[CONTRACT_COUNT]           = contractname;
  CONTRACT_AMOUNT[CONTRACT_COUNT]         = amount;
  CONTRACT_ISSUING_ENTITY[CONTRACT_COUNT] = entityId;
  CONTRACT_TYPE[CONTRACT_COUNT]           = contract_type;

  ENTITY_CONTRACTS[entityId][newContract] = CONTRACT_COUNT;
  CONTRACT_COUNT++;
}

#endif
