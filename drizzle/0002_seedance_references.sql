ALTER TABLE `files` ADD `duration_ms` integer;
--> statement-breakpoint
ALTER TABLE `jobs` ADD `progress` integer DEFAULT 0 NOT NULL;
