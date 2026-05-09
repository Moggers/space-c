#ifndef GAME_FACTION
#define GAME_FACTION

#include <stdint.h>
#include <string.h>

#define FACTIONS_MAX 1024
#define FACTIONS_MAX_ACCEPTED_CONTRACTS 16

uint32_t FACTION_COUNT = 0;
char *FACTION_NAMES[FACTIONS_MAX];
uint32_t FACTION_ACCEPTED_CONTRACTS[FACTIONS_MAX]
                                   [FACTIONS_MAX_ACCEPTED_CONTRACTS];

uint32_t faction_create(char *name) {
  FACTION_NAMES[FACTION_COUNT] = name;

  memset(FACTION_ACCEPTED_CONTRACTS[FACTION_COUNT], 0,
         sizeof(uint32_t) * FACTIONS_MAX_ACCEPTED_CONTRACTS);

  return FACTION_COUNT++;
}

#endif
