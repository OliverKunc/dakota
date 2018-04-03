function [f,g] = fbeamGV( x )%fbeamGV Cantilever beam with N design variables%        from Vanderplaats (1984) Example 5-1, pp. 147-150%  Vanderplaats, Garret N. (1984)%  Numerical Optimization Techniques for Engineering Design: With Applications%  McGraw-Hill, New Yorkx=x(:);ndv  = length(x);nelm = ndv/2;bi=1:nelm;hi=nelm+1:ndv;b = x(bi); % base width of each element in meters from cmh = x(hi); % height of each element in meters from cmP=50e3;    % NewtonsE=200e5;   % Newtons / cm^2 (Pascal x10^-4)L=500;     % Length in centimetersSigma_max = 14e3; % N/cm^2tip_max   = 2.5;  %0.03457; % centimetersInertia = b.*h.^3/12;ll = (L/nelm)*ones(nelm,1);suml = (1:nelm)'*L/nelm;% Calculate lateral slopes and deflections%% yp = zeros(size(ll)); y=zeros(size(ll)); M=zeros(size(ll));% yp(1) = P*ll(1)/(E*Inertia(1)) * (L - ll(1)/2);% y(1) = P*ll(1)^2/(2*E*Inertia(1)) * (L - ll(1)/3);% for i=2:nelm% 	suml = sum(ll(1:i));% 	yp(i) = yp(i-1) +                 P*ll(i)  /  (E*Inertia(i)) * (L +   ll(i)/2 - suml);% 	y(i)  = y(i-1)  + yp(i-1)*ll(i) + P*ll(i)^2/(2*E*Inertia(i)) * (L + 2*ll(i)/3 - suml);% end% g(nelm+1) = y(nelm) / tip_max - 1; % tip deflection constrainty1 = P*ll.^2 ./(2*E*Inertia) .* ( L - suml + 2/3*ll );yp = P*ll    ./  (E*Inertia) .* ( L - suml +     ll/2 );for i = 2:nelm    yp(i) = yp(i) + yp(i-1);endyN  = sum( y1 ) + sum( yp(1:end-1).*ll(2:end ) );M   = P * ( L - suml + ll );sigma = M.*(h/2)./Inertia;f = sum( b.*h.*ll );               % Volume of beam in cm^3g = zeros( 2*nelm+1, 1 );g(nelm+1) = yN    / tip_max - 1;   % tip deflection constraintg(1:nelm) = sigma / Sigma_max - 1; % stressesg(nelm+2:2*nelm+1) = (h - 20*b);   % aspect ratios (cm)end