# DarkMesh
Pentesting Meshtastic

# HW
It works with Heltec v3 by default. You can rewrite it to run on other platforms too.

# Contribution
Feel free to contribute, PRs are welcomed. If you add some features in your fork, please share it with us too.

# Compilation
Clone the project, open it with your ide, and start to compile. It'll run into an error. Then go to managed_components/jgromes__radiolib/src/protocols/PhysicalLayer/PhysicalLayer.h  and insert this to line 231: virtual ~PhysicalLayer() = default;
The PR is merged for this, but waiting for the next release. Till that, you can compile it this way.


This project depends on the https://github.com/htotoo/EspMeshtasticCompact you can contribute to that too!

# Using
Connect to Wifi: DarkMesh pass: 1234Dark
Open http://192.168.4.1/

# Attacks
- Flood with Nodes:  Floods the network with fake nodes, with fake coordinate.
- Namechange: Can rename a node or an already seen node in others list. Not permanent.
- Position poision: Fakes the coordinates of a node or an already seen node in others list. Not permanent.
- PKI poision: Advertises the node (selected or random) PKI with a replaced one. Making othesr see the node in red.
- DDOS: takes down the mesh network making it unusable. Recovers after stopped the attack. (as a side effect new nodes will appear)

> [!IMPORTANT] 
This project is intended for educational purposes only. Use it exclusively in controlled environments and **only** with explicit permission from all parties involved.  
Meshtastic is an excellent open-source project. Please respect its community and users—do not use this tool to cause harm or disrupt any Meshtastic networks.