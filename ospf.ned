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
    parameters:
        int numRouters = default(10);
    gates:
        inout gate[];
        inout clientGate;
}

network Ospf
{
    @display("bgb=664,628");
    submodules:
        r1: routerOspf {
            @display("p=167,201");
        }
        r2: routerOspf {
            @display("p=239,201");
        }
        r3: routerOspf {
            @display("p=317,201");
        }
        r4: routerOspf {
            @display("p=161,297");
        }
        r5: routerOspf {
            @display("p=239,297");
        }
        r6: routerOspf {
            @display("p=161,375");
        }
        r7: routerOspf {
            @display("p=239,375");
        }
        r8: routerOspf {
            @display("p=399,201");
        }
        r9: routerOspf {
            @display("p=317,297");
        }
        r10: routerOspf {
            @display("p=399,297");
        }
        c1: Client {
            @display("p=82,128");
        }
        c2: Client {
            @display("p=229,103");
        }
        c3: Client {
            @display("p=300,103");
        }
        c4: Client {
            @display("p=66,297");
        }
        c5: Client {
            @display("p=339,404");
        }
        c6: Client {
            @display("p=161,454");
        }
        c7: Client {
            @display("p=239,454");
        }
        c8: Client {
            @display("p=561,142");
        }
        c9: Client {
            @display("p=425,404");
        }
        c10: Client {
            @display("p=536,287");
        }
    connections:
        //network connections — delay 10ms
        r1.gate++ <--> { delay = 10ms; } <--> r2.gate++;
        r1.gate++ <--> { delay = 10ms; } <--> r4.gate++;

        r2.gate++ <--> { delay = 10ms; } <--> r3.gate++;
        r2.gate++ <--> { delay = 10ms; } <--> r4.gate++;
        r2.gate++ <--> { delay = 10ms; } <--> r5.gate++;

        r3.gate++ <--> { delay = 10ms; } <--> r8.gate++;
        r3.gate++ <--> { delay = 10ms; } <--> r9.gate++;

        r4.gate++ <--> { delay = 10ms; } <--> r5.gate++;
        r4.gate++ <--> { delay = 10ms; } <--> r6.gate++;

        r5.gate++ <--> { delay = 10ms; } <--> r7.gate++;
        r5.gate++ <--> { delay = 10ms; } <--> r9.gate++;

        r8.gate++ <--> { delay = 10ms; } <--> r10.gate++;

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
