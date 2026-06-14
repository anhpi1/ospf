
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
}
