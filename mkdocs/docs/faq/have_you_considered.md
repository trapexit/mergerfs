# "Have You Considered?"

## Have you considered limiting drive spinup?

Yes. See [Limiting Drive Spinup](limit_drive_spinup.md).


## Have you considered porting to MacOS?

Yes, but the FUSE implementation on MacOS has gotten a bit complicated
and fragmented. I'm not opposed to supporting it but given the low
demand, lack of the author having a modern MacOS system, the fact
mergerfs does not use standard
[libfuse](https://github.com/libfuse/libfuse) API, and MacOS FUSE
situation it is unlikely.


## Have you considered porting mergerfs to Windows (WinFsp)?

Similar to MacOS the demand is very low and mergerfs does not use the
[libfuse](https://github.com/libfuse/libfuse) library so porting to
WinFsp would first mean porting it to `libfuse`.
