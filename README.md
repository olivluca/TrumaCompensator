# High altitude compensator for a truma diesel heater

A truma diesel heater (the previous model with the square cowl) can only work up to 1500m of altitude, 
because, due to the reduction of oxygen in the atmosphere, it cannot burn completely the fuel, leaving
residues that eventually could lead to a failure of the burner.

The solution is to reduce the amount of fuel at high altitudes, the problem is how to achieve that. 

The fuel pump is a dosimetric pump: it's actually a solenoid that every time it's triggered it pumps a fixed amount
of fuel. To control the amount of fuel the heater changes the frequency of 12v pulses sent to the pump.

The "solution" proposed by Truma (or should I say ACME of Wyle E. Coyote fame?) is to put a second pump, 
with a lower flow rate, in parallel to the standard one and to manually switch on either one or the other.

This ~~botch job~~ kit (consisting of a pump, a couple meters of plastic tubing, two plastic Y fittings, a switch and a wiring harness) costs
[more than 600€](https://www.grossostore.eu/product/altitude-set-for-combi-d/) and the installation, if made by a professional, will cost about half that.

The new model of the heater (with the round cowl), instead of the Wyle E. Coyote solution, has a barometric sensor so it
can reduce the frequency of the pulses depending on the altitude.

This is what an old [espar/eberspacher high altitude kit](https://www.yumpu.com/en/document/view/19513829/high-altitude-compensator-05-2009pdf-espar-of-michigan) did: it intercepted the 12V for the pump coming 
from the heater and sent the pulses to the pump with a different frequency depending on the altitude.

This repository tries to do the same, using a bmp280 barometric sensor to read the altitude.

**WORK IN PROGRESS**
