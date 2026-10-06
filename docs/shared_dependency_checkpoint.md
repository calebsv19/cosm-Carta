# Carta shared renderer checkpoint

Prepared 2026-10-06 before the cross-program UI migration.

The exact committed upstream dependency available for the next managed import is
shared commit `09ff89a0a32d80981010b15a7d45a2cdf8e5fd9f`, with
`vk_renderer 1.7.0`, `vk_runtime 0.6.0`, `kit_ui 0.18.0`, `kit_pane 0.5.0`,
`kit_render 0.14.6`, and `kit_workspace_authoring 0.6.1`.
The additive renderer version checkpoints the ordered texture-corner API that
was working-tree-only when the earlier 1.6.0 shared pin was accepted.

Carta currently retains its `third_party/codework_shared` snapshot and its
documented 1.3.2 renderer backports. The separate texture-corner source is byte
identical to the committed 1.7.0 source. The command header also matches, but
the command source differs because Carta's older snapshot retains frame
submission implementations in that file. Upstream owns those implementations
in the separate frame-submission module, including improved fence and
out-of-date/suboptimal handling. Exact-file differences therefore need a semantic
review and a full clean rebuild at adoption; copying selected command files is
not the update procedure.

This feature checkpoint preserves existing vendor bytes. No subtree update has
occurred. The next managed import must use a bounded target manifest with
`bin/update_shared_subtrees.sh`, select the exact shared commit and record its
manifest. Keep the import commit separate from application integration fixes.
Requalify terrain texture corners, map projection, clipping, drawable changes,
presentation recovery and SDL fallback against the imported dependency.

Application VERSION 0.3.0, canonical source, stable Desktop app, personal data
and published region packs remain separate from these local source checkpoints.
The installed comparison app may retain its prior dirty-source identity after
the source commit; an identity change alone does not refresh the Desktop app.
