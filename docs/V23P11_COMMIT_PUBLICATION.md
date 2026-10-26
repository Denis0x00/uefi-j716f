# V23P11 commit publication diagnostics

P11A/B/D are built and host verified, unflashed. The independent profile is
`j716f-commit-trace-v23p11.dsc/.fdf`, output `Build/j716fCommitTraceV23P11Pkg`.
All nonpayload FV sections and compact SEC match P9/P8. The firmware version
string and reader remain P9; the embedded FAT menu identifies P11A/B/D.

The new terminal writer records P11S stage1–5 in phase11's value. The unchanged
reader shows **PRE_MMU**, but this value is a publication stage code, not SCTLR
or MMU-on evidence. A stops before original BCM STR, B after STR before ISB,
D at B's hook with only the terminal first DSB SY replaced by DMB SY.

The menu has reader, one manual kernel variant, **Reboot**, **Power off** and
Exit. Reader is default; both timeouts are disabled. Power actions use the
new independent `J716fPowerAction.efi`: query PSCI VERSION/FEATURES, then direct
SYSTEM_RESET or SYSTEM_OFF with zero unused arguments. No RESET2 or runtime
reset-driver change. Returned calls report failure. Both entries are host
verified; their hardware behavior is pending.

40 actual kernel, 18 linked-reader and 22 linked-power cases passed with
explicit hardware/UEFI mocks. Actual image validation and hashes:
[v23p11-validation.json](v23p11-validation.json).

Follow the device repository's `docs/V23P11_COMMIT_PUBLICATION.md`: boot_b only,
A first, fresh Query/ARM, one kernel launch, immediate read-only reader capture.
Progress codes are lower bounds; interrupted rewrites may leave no valid record.
Do not claim hardware barrier completion or reset attribution from host tests.
