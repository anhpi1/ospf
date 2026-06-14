
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
simple clientOspf
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
		client1: clientOspf;
		client2: clientOspf;
		client3: clientOspf;
		client4: clientOspf;
		client5: clientOspf;
		client6: clientOspf;
		client7: clientOspf;
		client8: clientOspf;
		client9: clientOspf;
		client10: clientOspf;
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
		client1.gate++ <--> r1.gate++;
		client2.gate++ <--> r2.gate++;
		client3.gate++ <--> r3.gate++;
		client4.gate++ <--> r4.gate++;
		client5.gate++ <--> r5.gate++;
		client6.gate++ <--> r6.gate++;
		client7.gate++ <--> r7.gate++;
		client8.gate++ <--> r8.gate++;
		client9.gate++ <--> r9.gate++;
		client10.gate++ <--> r10.gate++;
}
