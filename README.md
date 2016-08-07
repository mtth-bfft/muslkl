# muslkl

A unikernel builder based on Musl and LKL, designed to run any vanilla application inside an SGX enclave.

## Building

This project relies partly on the closed-source SGX-Musl project from [SeReCa (Secure Enclaves for REactive Cloud Applications)](http://www.serecaproject.eu), an European research project.

Access was kindly given to me to develop this Master thesis project, and is required to build it. Once you get access too, building the project is as simple as:

    git clone https://github.com/mtth-bfft/muslkl.git
    cd muslkl
    make

