#include <math.h>
#include <QDebug>
#include "zoneviewzone.h"
#include "player.h"
#include "carddragitem.h"
#include "carditem.h"
#include "pb/command_dump_zone.pb.h"
#include "pb/command_move_card.pb.h"
#include "pb/serverinfo_card.pb.h"
#include "pb/response_dump_zone.pb.h"
#include "pending_command.h"

ZoneViewZone::ZoneViewZone(Player *_p, CardZone *_origZone, int _numberCards, bool _revealZone, QGraphicsItem *parent)
	: SelectZone(_p, _origZone->getName(), false, false, true, parent, true), bRect(QRectF()), minRows(0), numberCards(_numberCards), origZone(_origZone), revealZone(_revealZone), sortByName(false), sortByType(false)
{
	if (!revealZone)
		origZone->setView(this);
}

ZoneViewZone::~ZoneViewZone()
{
	emit beingDeleted();
	qDebug("ZoneViewZone destructor");
	if (!revealZone)
		origZone->setView(NULL);
}

QRectF ZoneViewZone::boundingRect() const
{
	return bRect;
}

void ZoneViewZone::paint(QPainter */*painter*/, const QStyleOptionGraphicsItem */*option*/, QWidget */*widget*/)
{
}

void ZoneViewZone::initializeCards(const QList<const ServerInfo_Card *> &cardList)
{
	if (!cardList.isEmpty()) {
		for (int i = 0; i < cardList.size(); ++i)
			addCard(new CardItem(player, QString::fromStdString(cardList[i]->name()), cardList[i]->id(), revealZone, this), false, i);
		reorganizeCards();
	} else if (!origZone->contentsKnown()) {
		Command_DumpZone cmd;
		cmd.set_player_id(player->getId());
		cmd.set_zone_name(name.toStdString());
		cmd.set_number_cards(numberCards);
		
		PendingCommand *pend = player->prepareGameCommand(cmd);
		connect(pend, SIGNAL(finished(const Response &)), this, SLOT(zoneDumpReceived(const Response &)));
		player->sendGameCommand(pend);
	} else {
		const CardList &c = origZone->getCards();
		int number = numberCards == -1 ? c.size() : (numberCards < c.size() ? numberCards : c.size());
		for (int i = 0; i < number; i++) {
			CardItem *card = c.at(i);
			addCard(new CardItem(player, card->getName(), card->getId(), revealZone, this), false, i);
		}
		reorganizeCards();
	}
}

void ZoneViewZone::zoneDumpReceived(const Response &r)
{
	const Response_DumpZone &resp = r.GetExtension(Response_DumpZone::ext);
	const int respCardListSize = resp.zone_info().card_list_size();
	for (int i = 0; i < respCardListSize; ++i) {
		const ServerInfo_Card &cardInfo = resp.zone_info().card_list(i);
		CardItem *card = new CardItem(player, QString::fromStdString(cardInfo.name()), cardInfo.id(), revealZone, this);
		addCard(card, false, i);
	}
	
	reorganizeCards();
}

// Because of boundingRect(), this function must not be called before the zone was added to a scene.
void ZoneViewZone::reorganizeCards()
{
	int cardCount = cards.size();
	if (!origZone->contentsKnown())
		for (int i = 0; i < cardCount; ++i)
			cards[i]->setId(i);

	int cols = floor(sqrt((double) cardCount / 2));
	int rows = ceil((double) cardCount / cols);
	if (rows < 1)
		rows = 1;
	if (minRows == 0)
		minRows = rows;
	else if (rows < minRows) {
		rows = minRows;
		cols = ceil((double) cardCount / minRows);
	}
	if (cols < 2)
		cols = 2;
	
	qDebug() << "reorganizeCards: rows=" << rows << "cols=" << cols;

	CardList cardsToDisplay(cards);
	if (sortByName || sortByType)
		cardsToDisplay.sort((sortByName ? CardList::SortByName : 0) | (sortByType ? CardList::SortByType : 0));
	
	for (int i = 0; i < cardCount; i++) {
		CardItem *c = cardsToDisplay.at(i);
		qreal x = (i / rows) * CARD_WIDTH;
		qreal y = (i % rows) * CARD_HEIGHT / 3;
		c->setPos(x + 5, y + 5);
		c->setRealZValue(i);
	}

	optimumRect = QRectF(0, 0, qMax(cols, 3) * CARD_WIDTH + 10, ((rows - 1) * CARD_HEIGHT) / 3 + CARD_HEIGHT + 10);
	updateGeometry();
	emit optimumRectChanged();
}

void ZoneViewZone::setSortByName(int _sortByName)
{
	sortByName = _sortByName;
	reorganizeCards();
}

void ZoneViewZone::setSortByType(int _sortByType)
{
	sortByType = _sortByType;
	reorganizeCards();
}

void ZoneViewZone::addCardImpl(CardItem *card, int x, int /*y*/)
{
	cards.insert(x, card);
	card->setParentItem(this);
	card->update();
}

void ZoneViewZone::handleDropEvent(const QList<CardDragItem *> &dragItems, CardZone *startZone, const QPoint &/*dropPoint*/)
{
	Command_MoveCard cmd;
	cmd.set_start_zone(startZone->getName().toStdString());
	cmd.set_target_player_id(player->getId());
	cmd.set_target_zone(getName().toStdString());
	cmd.set_x(0);
	cmd.set_y(0);
	
	for (int i = 0; i < dragItems.size(); ++i)
		cmd.mutable_cards_to_move()->add_card()->set_card_id(dragItems[i]->getId());

	player->sendGameCommand(cmd);
}

void ZoneViewZone::removeCard(int position)
{
	if (position >= cards.size())
		return;

	CardItem *card = cards.takeAt(position);
	card->deleteLater();
	reorganizeCards();
}

void ZoneViewZone::setGeometry(const QRectF &rect)
{
	prepareGeometryChange();
	setPos(rect.x(), rect.y());
	bRect = QRectF(0, 0, rect.width(), rect.height());
}

QSizeF ZoneViewZone::sizeHint(Qt::SizeHint /*which*/, const QSizeF & /*constraint*/) const
{
	return optimumRect.size();
}
