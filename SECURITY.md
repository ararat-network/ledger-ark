# Coordinated Vulnerability Disclosure Policy

> **IMPORTANT**: *DO NOT* open public issues on this repository for security
> vulnerabilities.

This app authorises transactions against keys that control real funds, so a
vulnerability here is a fund-loss vulnerability. Report privately.

## Reporting

Use GitHub's private vulnerability reporting on this repository
(Security → Report a vulnerability), and include:

- the affected component (derivation, HRP handling, transaction parsing,
  display) and device targets
- reproduction steps or a proof of concept, ideally as a failing unit or
  Zemu test
- your assessment of impact: what a signer would approve that they did not
  intend to

We will acknowledge the report, keep you informed of progress, and credit you
in the fix's release notes unless you prefer otherwise. Please give us a
reasonable window to ship a fix before any public disclosure; a signing app
cannot be hot-patched on users' devices.

## Scope

This repository's application code (`app/src/`). Issues in the Ledger OS or
SDK belong to Ledger's own program; issues in the ark chain belong to the
chain repository's policy.
