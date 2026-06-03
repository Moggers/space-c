# AI ore hand-off writes the wrong inventory slot

- **Status:** open
- **Severity:** bug
- **Area:** ai

## What
When a docked AI ship transfers ore into the contract issuer's inventory
(`ai_ship_select_tasks`), it picks a `recipient_slot` on the issuer, then writes
the item type using the *ship's* slot index `k` instead of `recipient_slot`:

```c
ENTITY_INVENTORY_COUNT[issuer][recipient_slot] += ENTITY_INVENTORY_COUNT[ship][k];
ENTITY_INVENTORY_ITEMS[issuer][k] = ENTITY_INVENTORY_ITEMS[ship][k];  // wrong slot
ENTITY_INVENTORY_ITEMS[ship][k]  = 0;
```

The count lands in `recipient_slot` but the item *type* is written to
`issuer[k]`. If `k != recipient_slot`, the issuer ends up with a counted-but-
typeless slot and a typed-but-uncounted slot.

## Where
- `src/ai.h` — `ai_ship_select_tasks`, `CONTRACT_ORE` branch, docked transfer

## Fix
Write the item type to the same slot as the count:

```c
ENTITY_INVENTORY_ITEMS[issuer][recipient_slot] = ENTITY_INVENTORY_ITEMS[ship][k];
```

Also double-check the recipient-slot search picks the *last* match rather than the
first (it currently keeps overwriting `recipient_slot` in the loop without
breaking).
