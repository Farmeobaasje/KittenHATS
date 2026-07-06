## Description

Please include a summary of the change and which issue is fixed.

Fixes # (issue)

## Type of change

- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Documentation update

## Build Verification

- [ ] `make clean && make` passes with zero errors
- [ ] No warnings in `app/src/` code
- [ ] Payload size is < 256KB

## Hardware Test Status

- [ ] Tested on hardware
- [ ] Hardware tests pending (documented below)

## Checklist

- [ ] My code follows the project's code style
- [ ] I have not modified any code in `app/vendor/hekate/`
- [ ] I have not used `lv_label` (font rendering doesn't work in standalone payload)
- [ ] PIN fail-safe is maintained (incorrect PIN never leads to Nyx)
- [ ] Config fail-safe is maintained (no config → error on screen)
