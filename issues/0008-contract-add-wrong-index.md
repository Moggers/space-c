# `contract_add` indexes the wrong table dimension

- **Status:** open
- **Severity:** bug
- **Area:** contracts

## What
`contract_add` searches for a free per-entity contract slot but indexes the wrong
dimension of `ENTITY_CONTRACTS`:

```c
for (newContract = 0; newContract < MAX_CONTRACTS_PER_ENTITY; newContract++) {
  if (ENTITY_CONTRACTS[newContract][newContract] == 0) {   // wrong
    break;
  }
}
...
ENTITY_CONTRACTS[entityId][newContract] = CONTRACT_COUNT;   // writes here
```

The scan reads the diagonal `ENTITY_CONTRACTS[newContract][newContract]` (indexed
by *contract slot* in the *entity* dimension) instead of
`ENTITY_CONTRACTS[entityId][newContract]`. So the "find a free slot" logic
inspects unrelated entities' rows, and can pick an occupied slot or the wrong one.

## Where
- `src/contracts.h` — `contract_add`

## Fix
Scan the row for the actual issuing entity:

```c
for (newContract = 0; newContract < MAX_CONTRACTS_PER_ENTITY; newContract++) {
  if (ENTITY_CONTRACTS[entityId][newContract] == 0) break;
}
```

(Also consider guarding against the row being full.)
