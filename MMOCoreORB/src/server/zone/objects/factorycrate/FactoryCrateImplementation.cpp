/*
 * FactoryCrateImplementation.cpp
 *
 *  Created on: 07/25/2010
 *      Author: Kyle
 */

#include "server/zone/objects/factorycrate/FactoryCrate.h"
#include "server/zone/ZoneServer.h"
#include "server/zone/packets/factory/FactoryCrateObjectMessage3.h"
#include "server/zone/packets/factory/FactoryCrateObjectMessage6.h"
#include "server/zone/packets/factory/FactoryCrateObjectDeltaMessage3.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/managers/object/ObjectManager.h"
#include "server/zone/packets/scene/AttributeListMessage.h"
#include "server/zone/packets/chat/ChatSystemMessage.h"
#include "server/zone/objects/transaction/TransactionLog.h"

void FactoryCrateImplementation::initializeTransientMembers() {
	TangibleObjectImplementation::initializeTransientMembers();

	setLoggingName("FactoryCrate");
}

void FactoryCrateImplementation::loadTemplateData(SharedObjectTemplate* templateData) {
	TangibleObjectImplementation::loadTemplateData(templateData);


}

void FactoryCrateImplementation::sendBaselinesTo(SceneObject* player) {
	/*StringBuffer msg;
	msg << "sending cell number " << cellNumber << " baselines";
	info(msg.toString(), true);*/

	BaseMessage* fctyMsg3 = new FactoryCrateObjectMessage3(_this.getReferenceUnsafeStaticCast());
	player->sendMessage(fctyMsg3);

	BaseMessage* fctyMsg6 = new FactoryCrateObjectMessage6(_this.getReferenceUnsafeStaticCast());
	player->sendMessage(fctyMsg6);

}

void FactoryCrateImplementation::fillAttributeList(AttributeListMessage* alm, CreatureObject* player) {
	if (alm == nullptr || player == nullptr) {
		return;
	}

	Reference<TangibleObject*> prototype = getPrototype();

	if (prototype == nullptr || !prototype->isTangibleObject()) {
		error() << "Broken Factory Crate - ID: " << getObjectID() << " Name: " << getDisplayedName();

		return;
	}

	alm->insertAttribute("volume", 1);
	alm->insertAttribute("crafter", prototype->getCraftersName());
	alm->insertAttribute("serial_number", prototype->getSerialNumber());

	alm->insertAttribute("factory_count", getUseCount());
	alm->insertAttribute("factory_attribs", "\\#pcontrast2 --------------");

	alm->insertAttribute("object_type", prototype->getGameObjectTypeStringID());

	StringBuffer type;
	type << "@" << prototype->getObjectNameStringIdFile() << ":" << prototype->getObjectNameStringIdName();

	alm->insertAttribute("original_name", type);

	prototype->fillAttributeList(alm, player);
}

void FactoryCrateImplementation::fillObjectMenuResponse(ObjectMenuResponse* menuResponse, CreatureObject* player) {
	TangibleObjectImplementation::fillObjectMenuResponse(menuResponse, player);
}

int FactoryCrateImplementation::handleObjectMenuSelect(CreatureObject* player, byte selectedID) {

	switch (selectedID) {

	case 77:

		break;

	default:
		TangibleObjectImplementation::handleObjectMenuSelect(player, selectedID);
		break;
	}

	return 0;
}

Reference<TangibleObject*> FactoryCrateImplementation::getPrototype() {

	if(getContainerObjectsSize() == 0) {
		error("FactoryCrateImplementation::getPrototype there isn't an object in the container");
		return nullptr;
	}

	Reference<TangibleObject*> prototype = getContainerObject(0).castTo<TangibleObject*>();

	if(prototype == nullptr || !prototype->isTangibleObject()) {
		error("FactoryCrateImplementation::getPrototype has a nullptr or non-tangible item");
		return nullptr;
	}

	return prototype;
}

String FactoryCrateImplementation::getCraftersName() {

	Reference<TangibleObject*> prototype = getPrototype();

	if(prototype == nullptr || !prototype->isTangibleObject()) {
		error("FactoryCrateImplementation::getCraftersName has a nullptr or non-tangible item");
		return "";
	}

	return prototype->getCraftersName();
}

String FactoryCrateImplementation::getSerialNumber() {

	Reference<TangibleObject*> prototype = getPrototype();

	if(prototype == nullptr || !prototype->isTangibleObject()) {
		error("FactoryCrateImplementation::getCraftersSerial has a nullptr or non-tangible item");
		return "";
	}

	return prototype->getSerialNumber();
}

int FactoryCrateImplementation::getPrototypeUseCount() {
	Reference<TangibleObject*> prototype = getPrototype();

	if (prototype == nullptr)
		return 0;

	return prototype->getUseCount();
}

bool FactoryCrateImplementation::extractObjectToInventory(CreatureObject* player) {

	Locker locker(_this.getReferenceUnsafeStaticCast());

	if (!isValidFactoryCrate()) {
		error() << "extractObjectToInventory(player=" << player->getObjectID() << "): !isValidFactoryCrate() : " << *asSceneObject();
		return false;
	}

	if(getUseCount() < 1) {
		this->setUseCount(0, true);
		return false;
	}

	Reference<TangibleObject*> prototype = getPrototype();
	ManagedReference<SceneObject*> inventory = player->getSlottedObject("inventory").get();

	if (prototype == nullptr || !prototype->isTangibleObject() || inventory == nullptr) {
		error("FactoryCrateImplementation::extractObjectToInventory has a nullptr or non-tangible item");
		return false;
	}

	ObjectManager* objectManager = ObjectManager::instance();

	ManagedReference<TangibleObject*> protoclone = cast<TangibleObject*>( objectManager->cloneObject(prototype));

	if (protoclone != nullptr) {

		if(protoclone->hasAntiDecayKit()){
			protoclone->removeAntiDecayKit();
		}

		protoclone->setParent(nullptr);

		String errorDescription;
		int errorNumber = 0;

		if ((errorNumber = inventory->canAddObject(protoclone, -1, errorDescription)) != 0) {
			if (errorDescription.length() > 1) {
					player->sendMessage(new ChatSystemMessage(errorDescription));
			} else {
				inventory->error("cannot extratObjectToInventory " + String::valueOf(errorNumber));
			}

			protoclone->destroyObjectFromDatabase(true);

			return false;
		}

		/*
		 * I really didn't want to do this this way, but I had no other way of making the text on the crate be white
		 * if the item it contained has the yellow magic bit set. So I stripped the yellow magic bit off when the item is placed inside
		 * the crate, and added it back here.
		 */
		if(protoclone->getIsCraftedEnhancedItem()) {
			protoclone->addMagicBit(false);
		}

		TransactionLog trx(asSceneObject(), player, protoclone, TrxCode::EXTRACTCRATE);
		trx.addState("useCount", getUseCount() - 1);
		trx.addState("protoMapSize", protoclone->getContainerObjectsSize());

		if (protoclone->getContainerObjectsSize() > 0) {
			trx.setDebug(true);
			trx.addRelatedObject(protoclone->getObjectID(), true);
		}

		inventory->transferObject(protoclone, -1, true);
		inventory->broadcastObject(protoclone, true);

		setUseCount(getUseCount() - 1);

		return true;
	}

	return false;
}

Reference<TangibleObject*> FactoryCrateImplementation::extractObject() {
	Locker locker(_this.getReferenceUnsafeStaticCast());

	if (!isValidFactoryCrate()) {
		error() << "!isValidFactoryCrate " << getObjectNameStringIdName() << " ID: " << getObjectID();
		return nullptr;
	}

	Reference<TangibleObject*> prototype = getPrototype();

	if (prototype == nullptr || !prototype->isTangibleObject()) {
		error("FactoryCrateImplementation::extractObject has a nullptr or non-tangible item");
		return nullptr;
	}

	ObjectManager* objectManager = ObjectManager::instance();

	if (objectManager == nullptr)
		return nullptr;

	Reference<TangibleObject*> protoclone = cast<TangibleObject*>(objectManager->cloneObject(prototype));

	if (protoclone == nullptr) {
		return nullptr;
	}

	Locker protoLocker(protoclone, _this.getReferenceUnsafeStaticCast());

	if (protoclone->hasAntiDecayKit()){
		protoclone->removeAntiDecayKit();
	}

	// Some objects will have a use count of 0, so default to 1. Others like grenades can be greater than 1
	int prototypeUses = prototype->getUseCount();

	if (prototypeUses < 1)
		prototypeUses = 1;

	protoclone->setUseCount(prototypeUses, false);

	ManagedReference<SceneObject*> strongParent = getParent().get();

	if (strongParent == nullptr || !strongParent->transferObject(protoclone, -1, true, true)) {
		protoclone->destroyObjectFromDatabase(false);
		protoclone->destroyObjectFromWorld(false);
		return nullptr;
	}

	strongParent->broadcastObject(protoclone, true);

	// We extracted a single protoClone. Reduce crate use count
	decreaseUseCount();

	return protoclone;
}

void FactoryCrateImplementation::split(int newStackSize) {
	if (!isValidFactoryCrate()) {
		error() << "split(newStackSize=" << newStackSize << "): !isValidFactoryCrate(): " << *asSceneObject();
		return;
	}

	if (getUseCount() <= newStackSize)
		return;

	if(newStackSize > getUseCount())
		newStackSize = getUseCount();

	Reference<TangibleObject*> prototype = getPrototype();

	if(prototype == nullptr || !prototype->isTangibleObject()) {
		error("FactoryCrateImplementation::split has a nullptr or non-tangible item");
		return;
	}

	if(parent.get() == nullptr)
		return;

	ObjectManager* objectManager = ObjectManager::instance();

	ManagedReference<TangibleObject*> protoclone = cast<TangibleObject*>( objectManager->cloneObject(prototype));

	if(protoclone == nullptr)
		return;

	Locker plocker(protoclone);

	ManagedReference<FactoryCrate*> newCrate =
			(server->getZoneServer()->createObject(getServerObjectCRC(), 2)).castTo<FactoryCrate*>();

	if(newCrate == nullptr) {
		protoclone->destroyObjectFromDatabase(true);
		return;
	}

	protoclone->setParent(nullptr);

	Locker nlocker(newCrate);

	if (!newCrate->transferObject(protoclone, -1, true)) {
		protoclone->destroyObjectFromDatabase(true);
		newCrate->destroyObjectFromDatabase(true);
		return;
	}

	newCrate->setUseCount(newStackSize, false);
	newCrate->setCustomObjectName(getCustomObjectName(), false);

	ManagedReference<SceneObject*> strongParent = getParent().get();
	if (strongParent != nullptr) {
		if(	strongParent->transferObject(newCrate, -1, true)) {
			strongParent->broadcastObject(newCrate, true);
			setUseCount(getUseCount() - newStackSize, true);
		} else {
			newCrate->destroyObjectFromDatabase(true);
		}
	}
}

void FactoryCrateImplementation::setUseCount(uint32 newUseCount, bool notifyClient) {
	if (useCount == newUseCount)
		return;

	useCount = newUseCount;

	if (useCount < 1) {
		destroyObjectFromWorld(true);

		destroyObjectFromDatabase(true);

		return;
	}

	if (!notifyClient)
		return;

	FactoryCrateObjectDeltaMessage3* dfcty3 = new FactoryCrateObjectDeltaMessage3(_this.getReferenceUnsafeStaticCast());
	dfcty3->setQuantity(newUseCount);
	dfcty3->close();

	broadcastMessage(dfcty3, true);
}

bool FactoryCrateImplementation::isValidFactoryCrate() {
	auto prototype = getContainerObject(0).castTo<TangibleObject*>();

	if (prototype == nullptr) {
		return false;
	}

	if (prototype->getContainerObjectsSize() > 0) {
		return false;
	}

	return true;
}
