// ┌─────────────────────────────────────────────────────────┐
// │                    10 Router Network                    │
// │                                                         │
// │   R1 ─── R2 ─── R3 ─── R8                               │
// │   │   / │       │       │                               │
// │   │  /  │       │       │                               │
// │   │ /   │       │       │                               │
// │   R4 ─── R5 ─── R9      R10                             │
// │   │       │                                             │
// │   │       │                                             │
// │   R6      R7                                            │
// │                                                         │
// │   Mỗi router: [Client] → [OspfRouter]                   │
// └─────────────────────────────────────────────────────────┘

simple routerOspf
{
    gates:
        inout gate[];
        inout clientGate;
}

network Ospf
{
    submodules:
        r1: routerOspf;
        r2: routerOspf;
        r3: routerOspf;
        r4: routerOspf;
        r5: routerOspf;
        r6: routerOspf;
        r7: routerOspf;
        r8: routerOspf;
        r9: routerOspf;
        r10: routerOspf;
        c1: Client;
        c2: Client;
        c3: Client;
        c4: Client;
        c5: Client;
        c6: Client;
        c7: Client;
        c8: Client;
        c9: Client;
        c10: Client;
    connections:
        //network connections
        r1.gate++ <--> r2.gate++;
        r1.gate++ <--> r4.gate++;

        r2.gate++ <--> r3.gate++;
        r2.gate++ <--> r4.gate++;
        r2.gate++ <--> r5.gate++;

        r3.gate++ <--> r8.gate++;
        r3.gate++ <--> r9.gate++;

        r4.gate++ <--> r5.gate++;
        r4.gate++ <--> r6.gate++;

        r5.gate++ <--> r7.gate++;
        r5.gate++ <--> r9.gate++;

        r8.gate++ <--> r10.gate++;

        //client connections
        r1.clientGate <--> c1.gate;
        r2.clientGate <--> c2.gate;
        r3.clientGate <--> c3.gate;
        r4.clientGate <--> c4.gate;
        r5.clientGate <--> c5.gate;
        r6.clientGate <--> c6.gate;
        r7.clientGate <--> c7.gate;
        r8.clientGate <--> c8.gate;
        r9.clientGate <--> c9.gate;
        r10.clientGate <--> c10.gate;
}
