# contracts/ext: additive extensions (goes to contracts/ext/README.md)

`contracts/` is frozen at tag `contracts-v1.2`. Nobody edits files outside `contracts/ext/` after Phase 0, and nobody asks anyone for a contract change.

If your stream needs something the contracts do not give you, pick the first option that works:

1. **Adapter on your own side.** Convert, default or ignore in code you own. Most needs end here.
2. **Additive extension in your own file.** Add `contracts/ext/<A|B>-<name>.json` (a JSON Schema that extends a frozen one with optional fields, or a new JSONL record kind) or `contracts/ext/<A|B>-<name>.md` (a new devctl/message that no existing message uses). Then append one line to your `contracts/ext/CHANGES-<A|B>.md`:
   `| <date> | <file> | what was added | why | consumers must ignore it if unknown |`
3. **Descope.** If neither works, drop the feature to the next tier in `SCOPE.yaml` and log it in your `docs/decisions/<stream>.md`.

Rules that `tools/check_contract_compat.py` enforces against `contracts-v1.2`:
- no file under `contracts/` (outside `ext/`) changes at all;
- an `ext/` schema may only add optional properties or new `kind` values; it may not make an existing optional field required, remove an enum value, or reuse a reason code number;
- new binary messages use a new message type id from the range reserved for your stream (A: `0x0100-0x01FF`, B: `0x0200-0x02FF`) and never change the size of an existing struct;
- new reason codes use your stream's reserved range (A: `900-949`, B: `950-999`).

Consumers must ignore unknown optional fields and unknown record kinds. Producers must keep emitting every v1.2 field.
