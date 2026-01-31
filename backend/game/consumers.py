import json
import random
from channels.generic.websocket import AsyncWebsocketConsumer
from channels.db import database_sync_to_async
from django.utils import timezone


class RoomConsumer(AsyncWebsocketConsumer):
    """WebSocket consumer for voice room real-time communication"""

    async def connect(self):
        self.room_id = self.scope['url_route']['kwargs']['room_id']
        self.player_id = self.scope['url_route']['kwargs'].get('player_id')
        self.room_group_name = f'room_{self.room_id}'

        # Join room group
        await self.channel_layer.group_add(
            self.room_group_name,
            self.channel_name
        )

        await self.accept()

        # Get room data and notify others of join
        room_data = await self.get_room_data()
        player_data = await self.get_player_data()

        if player_data:
            # Broadcast player joined
            await self.channel_layer.group_send(
                self.room_group_name,
                {
                    'type': 'player_joined',
                    'player': player_data,
                    'room': room_data
                }
            )

    async def disconnect(self, close_code):
        # Leave room group
        player_data = await self.get_player_data()

        if player_data:
            # Broadcast player left
            await self.channel_layer.group_send(
                self.room_group_name,
                {
                    'type': 'player_left',
                    'player': player_data
                }
            )

        await self.channel_layer.group_discard(
            self.room_group_name,
            self.channel_name
        )

    async def receive(self, text_data):
        """Handle incoming messages from WebSocket"""
        data = json.loads(text_data)
        message_type = data.get('type')

        if message_type == 'chat_message':
            await self.handle_chat_message(data)
        elif message_type == 'game_action':
            await self.handle_game_action(data)
        elif message_type == 'call_number':
            await self.handle_call_number(data)
        elif message_type == 'mark_number':
            await self.handle_mark_number(data)
        elif message_type == 'claim_bingo':
            await self.handle_claim_bingo(data)
        elif message_type == 'start_game':
            await self.handle_start_game(data)
        elif message_type == 'player_progress':
            await self.handle_player_progress(data)
        elif message_type == 'mute_toggle':
            await self.handle_mute_toggle(data)

    async def handle_chat_message(self, data):
        """Handle chat messages"""
        message = data.get('message', '')
        player_data = await self.get_player_data()

        if player_data and message:
            # Save message to database
            await self.save_chat_message(message, False)

            # Broadcast to room
            await self.channel_layer.group_send(
                self.room_group_name,
                {
                    'type': 'chat_message',
                    'message': message,
                    'player': player_data,
                    'is_system': False,
                    'timestamp': timezone.now().isoformat()
                }
            )

    async def handle_start_game(self, data):
        """Handle game start"""
        # Start countdown and notify all players
        await self.channel_layer.group_send(
            self.room_group_name,
            {
                'type': 'game_starting',
                'countdown': 10
            }
        )

    async def handle_call_number(self, data):
        """Handle number being called"""
        number = data.get('number')
        called_numbers = data.get('called_numbers', [])

        await self.channel_layer.group_send(
            self.room_group_name,
            {
                'type': 'number_called',
                'number': number,
                'called_numbers': called_numbers
            }
        )

    async def handle_mark_number(self, data):
        """Handle player marking a number"""
        player_data = await self.get_player_data()
        number = data.get('number')
        card_id = data.get('card_id')

        await self.channel_layer.group_send(
            self.room_group_name,
            {
                'type': 'number_marked',
                'player': player_data,
                'number': number,
                'card_id': card_id
            }
        )

    async def handle_claim_bingo(self, data):
        """Handle bingo claim"""
        player_data = await self.get_player_data()
        valid = data.get('valid', False)

        await self.channel_layer.group_send(
            self.room_group_name,
            {
                'type': 'bingo_claimed',
                'player': player_data,
                'valid': valid
            }
        )

    async def handle_player_progress(self, data):
        """Handle player progress update"""
        player_data = await self.get_player_data()
        progress = data.get('progress', 0)

        await self.channel_layer.group_send(
            self.room_group_name,
            {
                'type': 'progress_update',
                'player': player_data,
                'progress': progress
            }
        )

    async def handle_mute_toggle(self, data):
        """Handle mute toggle"""
        player_data = await self.get_player_data()
        is_muted = data.get('is_muted', False)

        await self.channel_layer.group_send(
            self.room_group_name,
            {
                'type': 'mute_changed',
                'player': player_data,
                'is_muted': is_muted
            }
        )

    async def handle_game_action(self, data):
        """Handle generic game actions"""
        action = data.get('action')
        payload = data.get('payload', {})
        player_data = await self.get_player_data()

        await self.channel_layer.group_send(
            self.room_group_name,
            {
                'type': 'game_action',
                'action': action,
                'payload': payload,
                'player': player_data
            }
        )

    # Event handlers (receive from channel layer)
    async def player_joined(self, event):
        await self.send(text_data=json.dumps({
            'type': 'player_joined',
            'player': event['player'],
            'room': event.get('room')
        }))

    async def player_left(self, event):
        await self.send(text_data=json.dumps({
            'type': 'player_left',
            'player': event['player']
        }))

    async def chat_message(self, event):
        await self.send(text_data=json.dumps({
            'type': 'chat_message',
            'message': event['message'],
            'player': event['player'],
            'is_system': event.get('is_system', False),
            'timestamp': event.get('timestamp')
        }))

    async def game_starting(self, event):
        await self.send(text_data=json.dumps({
            'type': 'game_starting',
            'countdown': event['countdown']
        }))

    async def number_called(self, event):
        await self.send(text_data=json.dumps({
            'type': 'number_called',
            'number': event['number'],
            'called_numbers': event['called_numbers']
        }))

    async def number_marked(self, event):
        await self.send(text_data=json.dumps({
            'type': 'number_marked',
            'player': event['player'],
            'number': event['number'],
            'card_id': event['card_id']
        }))

    async def bingo_claimed(self, event):
        await self.send(text_data=json.dumps({
            'type': 'bingo_claimed',
            'player': event['player'],
            'valid': event['valid']
        }))

    async def progress_update(self, event):
        await self.send(text_data=json.dumps({
            'type': 'progress_update',
            'player': event['player'],
            'progress': event['progress']
        }))

    async def mute_changed(self, event):
        await self.send(text_data=json.dumps({
            'type': 'mute_changed',
            'player': event['player'],
            'is_muted': event['is_muted']
        }))

    async def game_action(self, event):
        await self.send(text_data=json.dumps({
            'type': 'game_action',
            'action': event['action'],
            'payload': event['payload'],
            'player': event['player']
        }))

    # Database helpers
    @database_sync_to_async
    def get_room_data(self):
        from game.models import VoiceRoom, RoomParticipant
        try:
            room = VoiceRoom.objects.get(room_id=self.room_id)
            participants = RoomParticipant.objects.filter(
                room=room, is_active=True
            ).select_related('player')

            return {
                'id': str(room.room_id),
                'name': room.name,
                'game_mode': room.game_mode,
                'entry_fee': room.entry_fee,
                'card_cost': room.card_cost,
                'max_players': room.max_players,
                'first_prize': room.first_prize,
                'second_prize': room.second_prize,
                'third_prize': room.third_prize,
                'players': [
                    {
                        'id': p.player.id,
                        'name': p.player.name,
                        'avatar': '👤',  # Default avatar emoji
                        'is_muted': p.is_muted,
                        'is_playing': p.is_playing,
                        'cards_purchased': p.cards_purchased
                    }
                    for p in participants
                ]
            }
        except VoiceRoom.DoesNotExist:
            return None

    @database_sync_to_async
    def get_player_data(self):
        from game.models import Player
        if not self.player_id:
            return None
        try:
            player = Player.objects.get(id=self.player_id)
            return {
                'id': player.id,
                'name': player.name,
                'avatar': '👤',  # Default avatar emoji
                'level': player.level
            }
        except Player.DoesNotExist:
            return None

    @database_sync_to_async
    def save_chat_message(self, message, is_system=False):
        from game.models import VoiceRoom, ChatMessage, Player
        try:
            room = VoiceRoom.objects.get(room_id=self.room_id)
            player = Player.objects.get(id=self.player_id) if self.player_id else None
            ChatMessage.objects.create(
                room=room,
                player=player,
                message=message,
                is_system=is_system
            )
        except (VoiceRoom.DoesNotExist, Player.DoesNotExist):
            pass
