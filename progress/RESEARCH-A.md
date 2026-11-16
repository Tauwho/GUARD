# Stream A research notes

Short notes: link, finding, how it was used.

- [SplitMix reference by Sebastiano Vigna](https://prng.di.unimi.it/splitmix.c): fixed-width unsigned generator, public-domain dedication; candidate for reproducible bounded host trial perturbations. Do not claim statistical independence or safety from PRNG choice.

- [Hypothesis strategies](https://hypothesis.readthedocs.io/en/latest/reference/strategies.html): generate bounded binary/numeric inputs including representable subnormals; tests retain the discovered subnormal parser regression.
- [pytest hook documentation](https://docs.pytest.org/en/stable/how-to/writing_hook_functions.html): custom file collector activates optional A8 module only with its real backend; normal tests are not filtered.
