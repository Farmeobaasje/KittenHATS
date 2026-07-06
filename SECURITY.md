# Security Policy

## Supported Versions

| Version | Supported          |
|---------|--------------------|
| v0.1.x  | ✅ Active development |

## Reporting a Vulnerability

KittenHATS is a bare-metal payload for the Nintendo Switch. Security concerns fall into two categories:

### 1. PIN Bypass / Child Safety

The PIN is **child-proofing, not real security**. Physical SD card access is the true control point. If you find a way to bypass the PIN screen or access Nyx without the correct PIN:

- Open an issue with the label `security`
- Describe the bypass method in detail
- Include Switch model, firmware version, and HATS version

### 2. Code Vulnerabilities

If you find a security vulnerability in the code (buffer overflow, memory corruption, etc.):

- **Do NOT** open a public issue
- Email the maintainers directly (see GitHub profile)
- Provide a detailed description and proof of concept
- Allow reasonable time for a fix before disclosure

## What to Expect

- Acknowledgment within 48 hours
- Status updates every 7 days
- Credit in release notes for verified reports (if desired)

## Scope

The following are **NOT** considered security issues:
- Physical SD card access (this is the real control point)
- Modchip/RCM access (this is the boot chain entry point)
- Hekate/HATS vulnerabilities (report to those projects)
- Nintendo Switch firmware vulnerabilities (report to Nintendo)

## Child Safety Note

KittenHATS is designed to prevent accidental access to destructive tools by a child. It is NOT designed to prevent access by a determined adversary with physical access to the device. The PIN is stored as a SHA-256 hash on the SD card — anyone with SD card access can read or replace the config.
