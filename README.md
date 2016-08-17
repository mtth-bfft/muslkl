# muslkl

**(Work in progress)**

A unikernel builder based on Musl and LKL, designed to run any vanilla application inside an SGX enclave.
This project depends mostly on LKL, Musl and a fork of SGX-Musl, which is itself a fork of Musl created by the [SeReCa (Secure Enclaves for REactive Cloud Applications)](http://www.serecaproject.eu) European research project. All these projects are independent and under different licenses.

## Building

To run tests or any networked application, you will need to create a network tap and a bridge (more steps are required if you want your application to be reachable from/reach other hosts). Just run the following commands as root:

    ip link add dev lklbr0 type bridge
    ip tuntap add dev tap0 mode tap user <your_username>
    ip link set dev tap0 master lklbr0 up
    ip addr add 10.0.1.254/24 dev lklbr0

If your *ip* command is not up-to-date, you need to use separate *tunctl* and *bridge-utils* packages instead of *ip*:

    tunctl -t tap0 -u <your_username>
    brctl addbr lklbr0
    brctl addif lklbr0 tap0
    ip addr add 10.0.1.1/24 dev lklbr0
    ip link set dev lklbr0 up

All dependencies are handled as Git submodules, but this project relies partly on a closed-source codebase. Access was kindly given to me to develop this project as part of my MSc thesis. Once you get access to it, building the whole project is as simple as:

    git clone https://github.com/mtth-bfft/muslkl.git
    cd muslkl
    make
