#include "OITDefinitions.fx"

void InsertionSort(inout ListNode a[MAX_PPLL_BUFFER_DEPTH], uint sz)
{
	[loop]
	for (int i = 1; i < sz; i++)
	{
		ListNode value = a[i];
		[loop]
		for (int j = i - 1; j >= 0 && a[j].DepthAndCoverage.x < value.DepthAndCoverage.x; j--)
			a[j + 1] = a[j];
		a[j + 1] = value;
	}
}