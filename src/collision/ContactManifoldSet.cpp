/********************************************************************************
* ReactPhysics3D physics library, http://www.reactphysics3d.com                 *
* Copyright (c) 2010-2018 Daniel Chappuis                                       *
*********************************************************************************
*                                                                               *
* This software is provided 'as-is', without any express or implied warranty.   *
* In no event will the authors be held liable for any damages arising from the  *
* use of this software.                                                         *
*                                                                               *
* Permission is granted to anyone to use this software for any purpose,         *
* including commercial applications, and to alter it and redistribute it        *
* freely, subject to the following restrictions:                                *
*                                                                               *
* 1. The origin of this software must not be misrepresented; you must not claim *
*    that you wrote the original software. If you use this software in a        *
*    product, an acknowledgment in the product documentation would be           *
*    appreciated but is not required.                                           *
*                                                                               *
* 2. Altered source versions must be plainly marked as such, and must not be    *
*    misrepresented as being the original software.                             *
*                                                                               *
* 3. This notice may not be removed or altered from any source distribution.    *
*                                                                               *
********************************************************************************/

// Libraries
#include "ContactManifoldSet.h"
#include "NarrowPhaseInfo.h"
#include "constraint/ContactPoint.h"
#include "ProxyShape.h"
#include "collision/ContactManifold.h"

using namespace reactphysics3d;

// Constructor
ContactManifoldSet::ContactManifoldSet(ProxyShape* shape1, ProxyShape* shape2,
                                       MemoryAllocator& memoryAllocator, const WorldSettings& worldSettings)
                   : mNbManifolds(0), mShape1(shape1),
                     mShape2(shape2), mMemoryAllocator(memoryAllocator), mManifolds(nullptr), mWorldSettings(worldSettings) {

    // Compute the maximum number of manifolds allowed between the two shapes
    mNbMaxManifolds = computeNbMaxContactManifolds(shape1->getCollisionShape(), shape2->getCollisionShape());
}

// Destructor
ContactManifoldSet::~ContactManifoldSet() {

    // Clear all the contact manifolds
    clear();
}

void ContactManifoldSet::addContactPoints(NarrowPhaseInfo* narrowPhaseInfo) {

    assert(narrowPhaseInfo->contactPoints != nullptr);

    // For each potential contact point to add
    ContactPointInfo* contactPoint = narrowPhaseInfo->contactPoints;
    while (contactPoint != nullptr) {

        // Look if the contact point correspond to an existing potential manifold
        // (if the contact point normal is similar to the normal of an existing manifold)
        ContactManifold* manifold = mManifolds;
        bool similarManifoldFound = false;
        while(manifold != nullptr) {

            // Get the first contact point
            const ContactPoint* point = manifold->getContactPoints();
            assert(point != nullptr);

            // If we have found a corresponding manifold for the new contact point
            // (a manifold with a similar contact normal direction)
            if (point->getNormal().dot(contactPoint->normal) >= mWorldSettings.cosAngleSimilarContactManifold) {

                // Add the contact point to the manifold
                manifold->addContactPoint(contactPoint);

               similarManifoldFound = true;

               break;
            }

            manifold = manifold->getNext();
        }

        // If we have not found an existing manifold with a similar contact normal
        if (!similarManifoldFound) {

            // Create a new contact manifold
            ContactManifold* manifold = createManifold();

            // Add the contact point to the manifold
            manifold->addContactPoint(contactPoint);
        }

        contactPoint = contactPoint->next;
    }

    // All the contact point info of the narrow-phase info have been moved
    // into the potential contacts of the overlapping pair. We can now
    // remove the contacts points from the narrow phase info object.
    narrowPhaseInfo->resetContactPoints();
}

// Return the total number of contact points in the set of manifolds
int ContactManifoldSet::getTotalNbContactPoints() const {
    int nbPoints = 0;

    ContactManifold* manifold = mManifolds;
    while (manifold != nullptr) {
        nbPoints += manifold->getNbContactPoints();

        manifold = manifold->getNext();
    }

    return nbPoints;
}

// Return the maximum number of contact manifolds allowed between to collision shapes
int ContactManifoldSet::computeNbMaxContactManifolds(const CollisionShape* shape1, const CollisionShape* shape2) {

    // If both shapes are convex
    if (shape1->isConvex() && shape2->isConvex()) {
        return mWorldSettings.nbMaxContactManifoldsConvexShape;

    }   // If there is at least one concave shape
    else {
        return mWorldSettings.nbMaxContactManifoldsConcaveShape;
    }
}

// Remove a contact manifold that is the least optimal (smaller penetration depth)
void ContactManifoldSet::removeNonOptimalManifold() {

    assert(mNbManifolds > mNbMaxManifolds);
    assert(mManifolds != nullptr);

    // Look for a manifold that is not new and with the smallest contact penetration depth.
    // At the same time, we also look for a new manifold with the smallest contact penetration depth
    // in case no old manifold exists.
    ContactManifold* minDepthManifold = nullptr;
    decimal minDepth = DECIMAL_LARGEST;
    ContactManifold* manifold = mManifolds;
    while (manifold != nullptr) {

        // Get the largest contact point penetration depth of the manifold
        const decimal depth = manifold->getLargestContactDepth();

        if (depth < minDepth) {
            minDepth = depth;
            minDepthManifold = manifold;
        }

        manifold = manifold->getNext();
    }

    // Remove the non optimal manifold
    assert(minDepthManifold != nullptr);
    removeManifold(minDepthManifold);
}

// Create a new contact manifold and add it to the set
ContactManifold* ContactManifoldSet::createManifold() {

    ContactManifold* manifold = new (mMemoryAllocator.allocate(sizeof(ContactManifold)))
                                        ContactManifold(mShape1, mShape2, mMemoryAllocator, mWorldSettings);
    manifold->setPrevious(nullptr);
    manifold->setNext(mManifolds);
    if (mManifolds != nullptr) {
        mManifolds->setPrevious(manifold);
    }
    mManifolds = manifold;

    mNbManifolds++;

    return manifold;
}

// Return the contact manifold with a similar contact normal.
// If no manifold has close enough contact normal, it returns nullptr
ContactManifold* ContactManifoldSet::selectManifoldWithSimilarNormal(const Vector3& contactNormal) const {

    ContactManifold* manifold = mManifolds;

    // Return the Id of the manifold with the same normal direction id (if exists)
    while (manifold != nullptr) {

        // Get the first contact point of the current manifold
        const ContactPoint* point = manifold->getContactPoints();
        assert(point != nullptr);

        // If the contact normal of the two manifolds are close enough
        if (contactNormal.dot(point->getNormal()) >= mWorldSettings.cosAngleSimilarContactManifold) {
            return manifold;
        }

        manifold = manifold->getNext();
    }

    return nullptr;
}

// Clear the contact manifold set
void ContactManifoldSet::clear() {

    ContactManifold* manifold = mManifolds;
    while(manifold != nullptr) {

        ContactManifold* nextManifold = manifold->getNext();

        // Delete the contact manifold
        manifold->~ContactManifold();
        mMemoryAllocator.release(manifold, sizeof(ContactManifold));

        manifold = nextManifold;

        mNbManifolds--;
    }

    mManifolds = nullptr;

    assert(mNbManifolds == 0);
}

// Remove a contact manifold from the set
void ContactManifoldSet::removeManifold(ContactManifold* manifold) {

    assert(mNbManifolds > 0);
	assert(manifold != nullptr);

    ContactManifold* previous = manifold->getPrevious();
    ContactManifold* next = manifold->getNext();

    if (previous != nullptr) {
        previous->setNext(next);
    }
	else {
		mManifolds = next;
	}

    if (next != nullptr) {
        next->setPrevious(previous);
    }

    // Delete the contact manifold
    manifold->~ContactManifold();
    mMemoryAllocator.release(manifold, sizeof(ContactManifold));
    mNbManifolds--;
}

// Make all the contact manifolds and contact points obsolete
void ContactManifoldSet::makeContactsObsolete() {

    ContactManifold* manifold = mManifolds;
    while (manifold != nullptr) {

        manifold->setIsObsolete(true, true);

        manifold = manifold->getNext();
    }
}

// Clear the obsolete contact manifolds and contact points
void ContactManifoldSet::clearObsoleteManifoldsAndContactPoints() {

    ContactManifold* manifold = mManifolds;
    while (manifold != nullptr) {

        // Get the next manifold in the linked-list
        ContactManifold* nextManifold = manifold->getNext();

        // If the manifold is obsolete
        if (manifold->getIsObsolete()) {

            // Delete the contact manifold
            removeManifold(manifold);
        }
        else {

            // Clear the obsolete contact points of the manifold
            manifold->clearObsoleteContactPoints();
        }

        manifold = nextManifold;
    }
}


// Remove some contact manifolds and contact points if there are too many of them
void ContactManifoldSet::reduce() {

    // Remove non optimal contact manifold while there are too many manifolds in the set
    while (mNbManifolds > mNbMaxManifolds) {
        removeNonOptimalManifold();
    }

    // Reduce all the contact manifolds in case they have too many contact points
    ContactManifold* manifold = mManifolds;
    while (manifold != nullptr) {
        manifold->reduce(mShape1->getLocalToWorldTransform());
        manifold = manifold->getNext();
    }
}
