# GNSS receiver runtime configuration

PurrGo will configure the GNSS receiver dynamically while running. The
generic API is independent of u-blox protocol details.

## Layers

    PurrGo policy
        |
        v
    generic GNSS configuration API
        |
        +-------------------------+
        |                         |
        v                         v
    u-blox 7 backend          modern u-blox backend
        |                         |
        +-----------+-------------+
                    v
                transport
                /       \
             PC COM   STM32 UART

The same UBX framing code can therefore be used on Windows and STM32.

## Verified u-blox 7 facts

The current PC receiver reports:

    HW       UBX-G70xx
    PROTVER  14.00

The u-blox 7 Protocol Version 14 specification documents:

* `UBX-CFG-RATE` (0x06 0x08): measurement/navigation rate.
* `UBX-CFG-GNSS` (0x06 0x3E): GNSS system/channel configuration.
* `UBX-CFG-RXM` (0x06 0x11): Continuous / Power Save Mode.
* `UBX-CFG-PM2` (0x06 0x3B): extended power-management parameters.
* `UBX-CFG-CFG` (0x06 0x09): save/load/clear configuration.

For `CFG-RATE`, Protocol 14 defines `measRate` in milliseconds,
`navRate` (fixed to 1) and `timeRef` (UTC or GPS).

For `CFG-RXM`, Protocol 14 defines `lpMode=0` as Continuous Mode and
`lpMode=1` as Power Save Mode.

The same specification states that Power Save Mode cannot be selected while
GLONASS processing is enabled. Therefore the application must not blindly
combine those settings.

## Constellation configuration

`CFG-GNSS` contains tracking-channel allocation fields and receiver
capabilities. PurrGo will therefore:

1. poll the current configuration;
2. parse the available channel information;
3. change only requested enable/disable states;
4. preserve valid allocations;
5. send the configuration;
6. wait for `UBX-ACK-ACK` or `UBX-ACK-NAK`;
7. verify the resulting state.

Until that poll/response path exists, the u-blox 7 constellation setter
returns failure instead of sending guessed data.

## Power management

"Sleep" is not represented as one universal u-blox command.

For u-blox 7, `CFG-RXM` selects Power Save Mode while `CFG-PM2` defines
power-management behavior. The final sleep/wake policy must be implemented
against the integration manual of the selected receiver module.

Modern u-blox receivers will have a separate backend because newer
generations expose different configuration interfaces, including
configuration-item based interfaces such as `UBX-CFG-VALSET`.

## Sources

u-blox 7 Receiver Description Including Protocol Specification, GPS.G7-SW-12001:
https://content.u-blox.com/sites/default/files/products/documents/u-blox7-V14_ReceiverDescriptionProtocolSpec_%28GPS.G7-SW-12001%29_Public.pdf

u-blox NEO-7 series:
https://www.u-blox.com/en/product/neo-7-series
