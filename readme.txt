The formula used is: 

\omega = 2\pi f

\Delta t_i = \frac{l_i}{v}

a = \sum_i{\frac{cos(\omega \Delta t_i )}{l_i^2}}

b = \sum_i{\frac{sin(\omega \Delta t_i )}{l_i^2}}

E = \pi (a^2 + b^2)

However, \frac{1}{l_i^2} is assumed to be nearly equal for all the sum members, so we drop it.

The formulas above in a C++-ish style:

omega = 2*pi*f;
d_t = l_i / v;
a = sum(cos(omega * d_t) / l_i^2);
b = sum(sin(omega * d_t) / l_i^2);
E = pi * (a^2 + b^2);

Also lambda, wavelength is:

	lambda = v / f
	
thus:
 
	f = v / lambda
	
and then:

	omega = 2 * pi * v / lambda 

And then:

	omega * d_t = 2 * pi * v / lambda  * l_i / v 
	
	omega * d_t = 2 * pi * l_i / lambda 
	
Let's define '2 * pi / lambda' as 'two_pi_inverse_lambda', then: 
	
	
	a = sum(cos(two_pi_inverse_lambda * l_i) / l_i^2);
	b = sum(sin(two_pi_inverse_lambda * l_i) / l_i^2);
	E = pi * (a^2 + b^2);

