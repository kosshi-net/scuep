CREATE TABLE playlist (
    id              INTEGER         NOT NULL    PRIMARY KEY,
    track_id        INTEGER         NOT NULL
);

CREATE TABLE variables (
    id              INTEGER         NOT NULL    PRIMARY KEY,
    key             TEXT            NOT NULL    UNIQUE,
    value           INTEGER
);

CREATE TABLE albums (
    id              INTEGER         NOT NULL    PRIMARY KEY,
    name            TEXT UNIQUE
);

CREATE TABLE artists (
    id              INTEGER         NOT NULL    PRIMARY KEY,
    name            TEXT UNIQUE
);

CREATE TABLE tracks (
    id              INTEGER         NOT NULL    PRIMARY KEY,

    url             TEXT UNIQUE     NOT NULL,

    title           TEXT            NOT NULL,
    album_id        INTEGER,
    artist_id       INTEGER,
    pcm_start       INTEGER,
    pcm_length      INTEGER,
    pcm_chapter     INTEGER,
    bitmask         INTEGER,
    FOREIGN KEY ( album_id  ) REFERENCES albums  ( id ),
    FOREIGN KEY ( artist_id ) REFERENCES artists ( id )
);


