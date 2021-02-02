SELECT (
	uri,
	title, 
	artist_id,
	album_id,
	pcm_start,
	pcm_length,
	pcm_chapter,
	bitmask
) FROM tracks WHERE (id=?1);

