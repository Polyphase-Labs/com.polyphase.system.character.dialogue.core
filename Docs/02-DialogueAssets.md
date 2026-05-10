# DialogueAsset format

`.dialogue` is the source format. Schema:

```json
{
  "start": "node_id",
  "speakers": [
    { "id": "string", "name": "string", "portrait": "asset", "voice": "asset" }
  ],
  "variables": [
    { "name": "string", "type": "bool|int|float|string", "default": <typed-value> }
  ],
  "nodes": [
    {
      "id": "string",
      "type": "line|choice|branch|event|set_variable|jump|end",
      "speaker": "string",
      "text": "string",
      "locKey": "string",
      "portrait": "asset_name",
      "voice": "asset_name",
      "eventName": "string",
      "jumpTargetId": "string",
      "tags": ["string", ...],
      "conditions": [{ "var": "name", "op": "equals|not_equals|greater|ge|less|le|exists|not_exists", "value": <typed-value> }],
      "choices": [
        {
          "id": "string",
          "text": "string",
          "locKey": "string",
          "target": "string",
          "tags": ["string", ...],
          "conditions": [...]
        }
      ],
      "variableOp": { "op": "set|add|sub|toggle", "var": "name", "value": <typed-value> }
    }
  ]
}
```

- `<typed-value>` is a JSON literal: bool / int / float / string.
- Comments (`//` and `/* */`) are tolerated in source files; on save the
  asset is a binary `.oct` and the original JSON is not preserved.
- `start` may be omitted; the importer falls back to the first node and warns.

## Binary serialization

`DialogueAsset::SaveStream` writes a versioned header (`DIALOGUE_ASSET_VERSION = 1`)
followed by speakers, variables, nodes, links, and a magic-anchored trailer
(`0xD1A107C0`) for forward-compat fields. `LoadStream` is symmetric.
