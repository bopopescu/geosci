function heights = Calc2dTreeHeight(qtree,D)
% Calc2dTreeHeight -- Measure the total height of a stat-quad-tree
%  Usage
%    heights = Calc2dTreeHeight(stree,D)
%  Inputs
%    stree     stat tree (e.g. generated by CalcStatTree)
%    D         maximum depth of search
%  Outputs
%    heights   Lengths of branches, assigning each branch
%              length equal to the entropy drop between parent & child
%
%  Description
%    This is a utility used by Plot2dBasisTree to set scale for plotting.
%    It is not intended for other use.
%
%  See Also
%        Calc2dStatTree, Plot2dBasisTree
%

	heights = zeros(size(qtree));
	value   = qtree;
	% prune, bottom-up, left-right scan
	for d=D-1:-1:0,
	   for bx=0:(2^d-1),
	   	for by=0:(2^d-1),
		
			vparent = qtree(qnode(d,bx,by));
			vchild  = value(qnode(d+1,2*bx  ,2*by)) + ...
			          value(qnode(d+1,2*bx+1,2*by)) + ...
					  value(qnode(d+1,2*bx  ,2*by+1)) + ...
					  value(qnode(d+1,2*bx+1,2*by+1));

			if(vparent < vchild),
				heights(qnode(d,bx,by))  = 0. ;
			else
				heights(qnode(d,bx,by))  = vparent - vchild;
			end

		 end
	   end
	end

%
% Copyright (c) 1993. David L. Donoho
%
    
    
 
 
%
%  Part of Wavelab Version 850
%  Built Tue Jan  3 13:20:41 EST 2006
%  This is Copyrighted Material
%  For Copying permissions see COPYING.m
%  Comments? e-mail wavelab@stat.stanford.edu 
