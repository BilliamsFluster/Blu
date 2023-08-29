#pragma once
#include <xhash>

namespace Blu
{
	// Class to represent a universally unique identifier (UUID)
	class UUID
	{
	public:
		// Default constructor
		UUID();

		// Constructor that initializes the UUID with a specific value
		UUID(uint64_t uuid);

		// Copy constructor is defaulted (it does the same thing as the automatically generated copy constructor)
		UUID(const UUID&) = default;

		// Operator overload to allow a UUID to be implicitly converted to a uint64_t
		operator uint64_t() const { return m_UUID; }

	private:
		// The underlying representation of the UUID
		uint64_t m_UUID;
	};
}

// The std namespace is being extended to provide a hash function for Blu::UUID
namespace std
{
	// Specialization of the std::hash struct for Blu::UUID
	template<>
	struct hash<Blu::UUID>
	{
		// Function call operator that calculates the hash of a Blu::UUID
		std::size_t operator() (const Blu::UUID& uuid) const
		{
			// The hash of a Blu::UUID is the same as the hash of a uint64_t
			// UUID is implicitly converted to a uint64_t
			return hash<uint64_t>()((uint64_t)uuid);
		}
	};
}





