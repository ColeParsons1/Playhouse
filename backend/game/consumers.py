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
        # WebRTC signaling
        elif message_type == 'webrtc_offer':
            await self.handle_webrtc_offer(data)
        elif message_type == 'webrtc_answer':
            await self.handle_webrtc_answer(data)
        elif message_type == 'webrtc_ice_candidate':
            await self.handle_webrtc_ice_candidate(data)
        elif message_type == 'webrtc_ready':
            await self.handle_webrtc_ready(data)
        # Music sync
        elif message_type == 'music_sync':
            await self.handle_music_sync(data)
        # Theme change
        elif message_type == 'theme_change':
            await self.handle_theme_change(data)

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

    # WebRTC signaling handlers
    async def handle_webrtc_ready(self, data):
        """Handle player announcing they're ready for WebRTC connections"""
        player_data = await self.get_player_data()
        await self.channel_layer.group_send(
            self.room_group_name,
            {
                'type': 'webrtc_ready',
                'player': player_data,
                'sender_channel': self.channel_name
            }
        )

    async def handle_webrtc_offer(self, data):
        """Handle WebRTC offer - forward to specific peer"""
        player_data = await self.get_player_data()
        target_id = data.get('target_id')
        offer = data.get('offer')

        await self.channel_layer.group_send(
            self.room_group_name,
            {
                'type': 'webrtc_offer',
                'offer': offer,
                'from_player': player_data,
                'target_id': target_id
            }
        )

    async def handle_webrtc_answer(self, data):
        """Handle WebRTC answer - forward to specific peer"""
        player_data = await self.get_player_data()
        target_id = data.get('target_id')
        answer = data.get('answer')

        await self.channel_layer.group_send(
            self.room_group_name,
            {
                'type': 'webrtc_answer',
                'answer': answer,
                'from_player': player_data,
                'target_id': target_id
            }
        )

    async def handle_webrtc_ice_candidate(self, data):
        """Handle ICE candidate - forward to specific peer"""
        player_data = await self.get_player_data()
        target_id = data.get('target_id')
        candidate = data.get('candidate')

        await self.channel_layer.group_send(
            self.room_group_name,
            {
                'type': 'webrtc_ice_candidate',
                'candidate': candidate,
                'from_player': player_data,
                'target_id': target_id
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

    async def handle_music_sync(self, data):
        """Handle music sync messages - broadcast to all players"""
        player_data = await self.get_player_data()

        await self.channel_layer.group_send(
            self.room_group_name,
            {
                'type': 'music_sync',
                'action': data.get('action'),
                'videoId': data.get('videoId'),
                'title': data.get('title'),
                'thumbnail': data.get('thumbnail'),
                'startTime': data.get('startTime', 0),
                'time': data.get('time'),
                'track': data.get('track'),
                'queue': data.get('queue'),
                'from_player': player_data,
                'sender_channel': self.channel_name
            }
        )

    async def handle_theme_change(self, data):
        """Handle theme change - broadcast to all players"""
        player_data = await self.get_player_data()

        await self.channel_layer.group_send(
            self.room_group_name,
            {
                'type': 'theme_change',
                'theme': data.get('theme'),
                'from_player': player_data,
                'sender_channel': self.channel_name
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

    async def music_sync(self, event):
        """Forward music sync to all players except sender"""
        if event.get('sender_channel') != self.channel_name:
            await self.send(text_data=json.dumps({
                'type': 'music_sync',
                'action': event['action'],
                'videoId': event.get('videoId'),
                'title': event.get('title'),
                'thumbnail': event.get('thumbnail'),
                'startTime': event.get('startTime', 0),
                'time': event.get('time'),
                'track': event.get('track'),
                'queue': event.get('queue'),
                'from_player': event.get('from_player')
            }))

    async def theme_change(self, event):
        """Forward theme change to all players except sender"""
        if event.get('sender_channel') != self.channel_name:
            await self.send(text_data=json.dumps({
                'type': 'theme_change',
                'theme': event.get('theme'),
                'from_player': event.get('from_player')
            }))

    # WebRTC signaling event handlers
    async def webrtc_ready(self, event):
        """Forward WebRTC ready signal to all peers except sender"""
        if event.get('sender_channel') != self.channel_name:
            await self.send(text_data=json.dumps({
                'type': 'webrtc_ready',
                'player': event['player']
            }))

    async def webrtc_offer(self, event):
        """Forward WebRTC offer to target peer only"""
        if self.player_id and str(event.get('target_id')) == str(self.player_id):
            await self.send(text_data=json.dumps({
                'type': 'webrtc_offer',
                'offer': event['offer'],
                'from_player': event['from_player']
            }))

    async def webrtc_answer(self, event):
        """Forward WebRTC answer to target peer only"""
        if self.player_id and str(event.get('target_id')) == str(self.player_id):
            await self.send(text_data=json.dumps({
                'type': 'webrtc_answer',
                'answer': event['answer'],
                'from_player': event['from_player']
            }))

    async def webrtc_ice_candidate(self, event):
        """Forward ICE candidate to target peer only"""
        if self.player_id and str(event.get('target_id')) == str(self.player_id):
            await self.send(text_data=json.dumps({
                'type': 'webrtc_ice_candidate',
                'candidate': event['candidate'],
                'from_player': event['from_player']
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
