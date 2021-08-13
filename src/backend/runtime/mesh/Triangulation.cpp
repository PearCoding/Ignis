#include "Triangulation.h"
#include "Tangent.h"

#include <numeric>

namespace IG {
std::vector<int> Triangulation::triangulate(const std::vector<Vector3f>& vertices)
{
	if (vertices.size() < 3)
		return {};

	// Get surface normal
	Vector3f N = Vector3f::Zero();
	for (size_t i = 0; i < vertices.size() - 1; ++i)
		N += vertices[i].cross(vertices[i + 1]);
	N += vertices.back().cross(vertices.front());

	N.normalize();

	Vector3f Nx, Ny;
	Tangent::frame(N, Nx, Ny);

	// Project to 2d
	std::vector<Vector2f> vertices2d;
	vertices2d.reserve(vertices.size());
	for (const Vector3f& v : vertices) {
		Vector3f pv = Tangent::toTangentSpace(N, Nx, Ny, v);
		vertices2d.push_back(Vector2f(pv(0), pv(1))); // Drop z component
	}

	return triangulate(vertices2d);
}

// Based on https://www.flipcode.com/archives/Efficient_Polygon_Triangulation.shtml

/// Compute area of polygon given by vertices
static float computeArea(const std::vector<Vector2f>& vertices)
{
	const size_t n = vertices.size();

	float area = 0.0f;
	for (size_t p = n - 1, q = 0; q < n; p = q++) {
		const Vector2f a = vertices[p];
		const Vector2f b = vertices[q];
		area += a(0) * b(1) - a(1) * b(0);
	}

	return area * 0.5f;
}

/// Check if point P is inside triangle defined by A, B and C
static bool insideTriangle(const Vector2f& A, const Vector2f& B, const Vector2f& C, const Vector2f& P)
{
	const Vector2f a  = C - B;
	const Vector2f b  = A - C;
	const Vector2f c  = B - A;
	const Vector2f ap = P - A;
	const Vector2f bp = P - B;
	const Vector2f cp = P - C;

	const float abp = a(0) * bp(1) - a(1) * bp(0);
	const float cap = c(0) * ap(1) - c(1) * ap(0);
	const float bcp = b(0) * cp(1) - b(1) * cp(0);

	return (abp >= 0.0f) && (bcp >= 0.0f) && (cap >= 0.0f);
}

/// Check if triangle configuration is valid
static bool canSnip(const std::vector<Vector2f>& vertices, int u, int v, int w, const std::vector<int>& order)
{
	const Vector2f A = vertices[order[u]];
	const Vector2f B = vertices[order[v]];
	const Vector2f C = vertices[order[w]];

	const float area = (B(0) - A(0)) * (C(1) - A(1)) - (B(1) - A(1)) * (C(0) - A(0));
	if (area <= 1e-5f)
		return false;

	for (int i = 0; i < (int)vertices.size(); ++i) {
		if ((i == u) || (i == v) || (i == w))
			continue;

		const Vector2f P = vertices[order[i]];
		if (insideTriangle(A, B, C, P))
			return false;
	}

	return true;
}

std::vector<int> Triangulation::triangulate(const std::vector<Vector2f>& vertices)
{
	if (vertices.size() < 3)
		return {};

	// Setup order
	std::vector<int> order(vertices.size());
	std::iota(std::begin(order), std::end(order), 0);

	// Flip if not counter clockwise
	if (computeArea(vertices) < 0)
		std::reverse(std::begin(order), std::end(order));

	int nv	  = (int)vertices.size();
	int count = 2 * nv; // Used for error detection

	std::vector<int> indices;
	indices.reserve(3 * (vertices.size() - 2));
	for (int m = 0, v = nv - 1; nv > 2;) {
		// if we loop, it is probably a non-simple polygon
		if (0 >= (count--))
			return {};

		// three consecutive vertices in current polygon, <u,v,w>
		int u = v;
		if (nv <= u)
			u = 0; // previous
		v = u + 1;
		if (nv <= v)
			v = 0; // new v
		int w = v + 1;
		if (nv <= w)
			w = 0; // next

		if (canSnip(vertices, u, v, w, order)) {
			// Push triangle indices
			indices.push_back(order[u]);
			indices.push_back(order[v]);
			indices.push_back(order[w]);

			++m;

			// remove v from remaining polygon
			for (int s = v, t = v + 1; t < nv; s++, t++)
				order[s] = order[t];
			nv--;

			// resest error detection counter
			count = 2 * nv;
		}
	}

	return indices;
}
} // namespace IG